/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotVendors.cpp
 *
 * Торговцы — отдельные боты, помеченные в БД (mod_ai_bots_vendors:
 * botName + предмет/цена/запас). Боты входят в мир, стоят у банка
 * своей фракции; игроки смотрят товары (.bot shop) и покупают (.bot buy).
 */

#include "BotVendors.h"
#include "BotAI.h"
#include "BotMgr.h"
#include "Config_Bots.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldSession.h"

#include <cmath>
#include <map>
#include <set>

namespace
{
    // guid бота-торговца -> список товаров
    std::map<ObjectGuid::LowType, std::vector<BotVendorItem>> s_stocks;
    std::vector<ObjectGuid::LowType> s_vendorGuids;
    std::map<ObjectGuid::LowType, uint8> s_vendorRace;
    std::map<ObjectGuid::LowType, uint32> s_reenterTimer;
    std::map<ObjectGuid::LowType, uint32> s_placeTimer;

    struct VendorSpot
    {
        uint32 mapId;
        float x;
        float y;
        float z;
        float o;
    };

    // Прилавки: Альянс — Штормград (у банка), Орда — Оргриммар (у банка).
    VendorSpot const AllianceSpot = { 0, -8833.0f,  622.0f, 94.35f, 4.70f };
    VendorSpot const HordeSpot    = { 1,  1629.0f, -4373.0f, 31.20f, 0.00f };

    bool IsAllianceRace(uint8 race)
    {
        switch (race)
        {
            case RACE_HUMAN:
            case RACE_DWARF:
            case RACE_NIGHTELF:
            case RACE_GNOME:
            case RACE_DRAENEI:
                return true;
            default:
                return false;
        }
    }

    VendorSpot const& SpotForRace(uint8 race)
    {
        return IsAllianceRace(race) ? AllianceSpot : HordeSpot;
    }
}

namespace AiBotsVendors
{
    void SpawnAll()
    {
        if (!AiBotsConfig::IsVendorsEnabled())
            return;

        s_stocks.clear();
        s_vendorGuids.clear();
        s_vendorRace.clear();
        s_reenterTimer.clear();
        s_placeTimer.clear();

        // Загружаем товары из БД.
        QueryResult result = WorldDatabase.Query(
            "SELECT `botName`, `itemId`, `price`, `stock` FROM `mod_ai_bots_vendors`");
        if (!result)
        {
            LOG_WARN("server.loading", ">> mod-ai-bots: таблица mod_ai_bots_vendors пуста или отсутствует — торговцы не имеют товаров.");
            return;
        }

        // Собираем товары по именам ботов.
        std::map<std::string, std::vector<BotVendorItem>> byName;
        do
        {
            Field* fields = result->Fetch();
            byName[fields[0].Get<std::string>()].push_back(
                { fields[1].Get<uint32>(), fields[2].Get<uint32>(), fields[3].Get<uint32>() });
        } while (result->NextRow());

        uint32 spawned = 0;
        for (auto const& pair : byName)
        {
            if (spawned >= AiBotsConfig::GetVendorsCount())
                break;
            ++spawned;

            CharacterCacheEntry const* entry = sCharacterCache->GetCharacterCacheByName(pair.first);
            if (!entry)
            {
                LOG_WARN("server.loading", ">> mod-ai-bots: торговец '{}' не найден среди персонажей.", pair.first);
                continue;
            }

            ObjectGuid::LowType guid = entry->Guid.GetCounter();

            uint8 race = RACE_HUMAN;
            if (QueryResult row = CharacterDatabase.Query("SELECT `race` FROM `characters` WHERE `guid` = {}", guid))
                race = row->Fetch()[0].Get<uint8>();

            s_stocks[guid] = pair.second;
            s_vendorGuids.push_back(guid);
            s_vendorRace[guid] = race;

            LOG_INFO("server.loading", ">> mod-ai-bots: торговец '{}' (guid {}) с {} товарами.",
                pair.first, guid, pair.second.size());

            // Входим в мир: старым персонажам с account=0 аккаунт выдаётся автоматически.
            if (!sBotMgr->EnterBotWorld(guid))
                LOG_ERROR("server.loading", ">> mod-ai-bots: торговец '{}' (guid {}) не смог войти в мир.", pair.first, guid);
        }
    }

    void Update(uint32 diff)
    {
        for (auto& pair : s_reenterTimer)
            pair.second = (pair.second > diff) ? pair.second - diff : 0;
        for (auto& pair : s_placeTimer)
            pair.second = (pair.second > diff) ? pair.second - diff : 0;

        for (ObjectGuid::LowType guid : s_vendorGuids)
        {
            Player* vendor = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
            if (!vendor || !vendor->IsInWorld())
            {
                uint32& timer = s_reenterTimer[guid];
                if (!timer)
                {
                    timer = 15000;
                    sBotMgr->EnterBotWorld(guid);
                }
                continue;
            }

            s_reenterTimer[guid] = 0;

            if (!vendor->IsAlive())
                vendor->ResurrectPlayer(1.0f, false);

            // Торговец стоит у прилавка, а не путешествует.
            if (BotAI* ai = sBotMgr->GetBotAI(guid))
                ai->SetAutonomous(false);

            auto raceIt = s_vendorRace.find(guid);
            VendorSpot const& spot = SpotForRace(raceIt != s_vendorRace.end() ? raceIt->second : RACE_HUMAN);

            float dx = vendor->GetPositionX() - spot.x;
            float dy = vendor->GetPositionY() - spot.y;
            float dz = vendor->GetPositionZ() - spot.z;
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (vendor->GetMapId() != spot.mapId || dist > 40.0f)
            {
                uint32& cooldown = s_placeTimer[guid];
                if (cooldown)
                    continue;

                cooldown = 3000;

                float const x = spot.x + float(guid % 3) * 2.0f;
                float const y = spot.y + float(guid % 2) * 2.0f;

                if (vendor->GetMapId() != spot.mapId)
                {
                    // Другая карта — дальний телепорт; подтверждать некому, завершаем сами.
                    if (vendor->TeleportTo(spot.mapId, x, y, spot.z, spot.o) && vendor->IsBeingTeleportedFar())
                        if (WorldSession* session = vendor->GetSession())
                            session->HandleMoveWorldportAck();
                }
                else
                {
                    vendor->NearTeleportTo(x, y, spot.z, spot.o);
                    if (vendor->IsBeingTeleportedNear())
                    {
                        vendor->SetSemaphoreTeleportNear(0);
                        WorldLocation const& dest = vendor->GetTeleportDest();
                        vendor->UpdatePosition(dest, true);
                        vendor->SetFallInformation(GameTime::GetGameTime().count(), dest.GetPositionZ());
                    }
                }

                LOG_INFO("server.loading", ">> mod-ai-bots: торговец '{}' поставлен к прилавку (карта {}, {:.0f} {:.0f}).",
                    vendor->GetName(), spot.mapId, x, y);
            }
        }
    }

    bool IsVendor(ObjectGuid::LowType botGuid)
    {
        return s_stocks.find(botGuid) != s_stocks.end();
    }

    Player* FindNearest(Player* seeker, float range)
    {
        if (!seeker || !seeker->IsInWorld())
            return nullptr;

        Player* best = nullptr;
        float bestDist = range;

        for (ObjectGuid::LowType guid : s_vendorGuids)
        {
            Player* vendor = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
            if (!vendor || !vendor->IsInWorld() || !vendor->IsAlive())
                continue;
            if (vendor->GetMapId() != seeker->GetMapId())
                continue;

            float dx = vendor->GetPositionX() - seeker->GetPositionX();
            float dy = vendor->GetPositionY() - seeker->GetPositionY();
            float dz = vendor->GetPositionZ() - seeker->GetPositionZ();
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (dist < bestDist)
            {
                bestDist = dist;
                best = vendor;
            }
        }

        return best;
    }

    std::vector<BotVendorItem> const* GetStock(ObjectGuid::LowType botGuid)
    {
        auto it = s_stocks.find(botGuid);
        return it != s_stocks.end() ? &it->second : nullptr;
    }

    std::string Buy(Player* buyer, ObjectGuid::LowType vendorGuid, uint32 itemIndex)
    {
        auto it = s_stocks.find(vendorGuid);
        if (it == s_stocks.end() || itemIndex >= it->second.size())
            return "Торговец не найден или неверный номер товара.";

        BotVendorItem& item = it->second[itemIndex];

        // stock==0 трактуем как бесконечный запас — проверка не требуется,
        // ограниченный запас уменьшается после покупки.
        if (!buyer || buyer->GetMoney() < item.price)
            return "Недостаточно золота.";

        ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(item.itemId);
        if (!tmpl)
            return "Предмет не существует.";

        // Списание золота и выдача предмета.
        buyer->ModifyMoney(-static_cast<int32>(item.price));

        ItemPosCountVec dest;
        if (buyer->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item.itemId, 1) == EQUIP_ERR_OK)
        {
            Item* stored = buyer->StoreNewItem(dest, item.itemId, true);
            if (stored)
                buyer->SendNewItem(stored, 1, true, false, true);
        }
        else
        {
            buyer->ModifyMoney(static_cast<int32>(item.price));
            return "Нет места в сумке.";
        }

        // Уменьшаем запас, если он ограничен.
        if (item.stock > 0)
            --item.stock;

        return ""; // успех
    }
}
