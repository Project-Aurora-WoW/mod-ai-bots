/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotLoot.cpp
 *
 * Лут: золото с убитых мобов и случайные предметы. Лучшая экипировка
 * надевается, остальное бот продаёт ближайшему NPC-торговцу (в радиусе).
 * Если торговца рядом нет — вещь лежит в «сумке» бота до следующей продажи.
 */

#include "BotLoot.h"
#include "BotGuild.h"
#include "Config_Bots.h"

#include "Cell.h"
#include "CellImpl.h"
#include "Creature.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "Unit.h"

#include <list>
#include <map>
#include <vector>

namespace
{
    // Виртуальные кошельки ботов (guid -> медяки).
    std::map<ObjectGuid::LowType, uint32> s_wallets;

    struct PendingLoot
    {
        uint32 entry;
        uint32 price;
    };

    // Непроданные вещи, ждущие торговца.
    std::map<ObjectGuid::LowType, std::vector<PendingLoot>> s_stash;

    uint32 const BaseGoldPerKill = 5;   // базовое золото за моба
    uint32 const ItemDropChance = 8;    // шанс выпадения предмета, %
    float const VendorRange = 40.0f;    // радиус поиска торговца
    size_t const MaxStash = 20;         // сколько вещей бот носит до продажи

    // Максимальный тип брони, который может носить класс.
    uint8 MaxArmorSubclass(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:
            case CLASS_DEATH_KNIGHT: return ITEM_SUBCLASS_ARMOR_PLATE;
            case CLASS_HUNTER:
            case CLASS_SHAMAN:       return ITEM_SUBCLASS_ARMOR_MAIL;
            case CLASS_ROGUE:
            case CLASS_DRUID:        return ITEM_SUBCLASS_ARMOR_LEATHER;
            default:                 return ITEM_SUBCLASS_ARMOR_CLOTH;
        }
    }

    // Попытка подобрать случайный предмет-экипировку, подходящий боту.
    ItemTemplate const* RollRandomEquipment(Player* bot)
    {
        // До 60 попыток найти существующий предмет оружия/брони,
        // который бот сможет надеть по уровню, классу и типу брони.
        for (int attempt = 0; attempt < 60; ++attempt)
        {
            uint32 entry = urand(1, 45000);
            ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(entry);
            if (!tmpl || (tmpl->Class != ITEM_CLASS_WEAPON && tmpl->Class != ITEM_CLASS_ARMOR))
                continue;
            if (tmpl->ItemLevel <= 0 || tmpl->RequiredLevel > bot->GetLevel())
                continue;
            if (tmpl->AllowableClass > 0 && !(tmpl->AllowableClass & (1 << (bot->getClass() - 1))))
                continue;
            if (tmpl->Class == ITEM_CLASS_ARMOR && tmpl->SubClass > MaxArmorSubclass(bot->getClass()))
                continue;

            return tmpl;
        }
        return nullptr;
    }

    // Слот экипировки, в который встанет предмет.
    uint8 GetEquipSlotFor(ItemTemplate const* tmpl)
    {
        switch (tmpl->InventoryType)
        {
            case INVTYPE_HEAD:        return EQUIPMENT_SLOT_HEAD;
            case INVTYPE_SHOULDERS:   return EQUIPMENT_SLOT_SHOULDERS;
            case INVTYPE_CHEST:
            case INVTYPE_ROBE:        return EQUIPMENT_SLOT_CHEST;
            case INVTYPE_LEGS:        return EQUIPMENT_SLOT_LEGS;
            case INVTYPE_FEET:        return EQUIPMENT_SLOT_FEET;
            case INVTYPE_HANDS:       return EQUIPMENT_SLOT_HANDS;
            case INVTYPE_WAIST:       return EQUIPMENT_SLOT_WAIST;
            case INVTYPE_WRISTS:      return EQUIPMENT_SLOT_WRISTS;
            case INVTYPE_CLOAK:       return EQUIPMENT_SLOT_BACK;
            case INVTYPE_WEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_2HWEAPON:    return EQUIPMENT_SLOT_MAINHAND;
            case INVTYPE_SHIELD:
            case INVTYPE_HOLDABLE:    return EQUIPMENT_SLOT_OFFHAND;
            case INVTYPE_RANGED:
            case INVTYPE_RANGEDRIGHT:
            case INVTYPE_THROWN:
            case INVTYPE_RELIC:       return EQUIPMENT_SLOT_RANGED;
            default:                  return EQUIPMENT_SLOT_END;
        }
    }

    bool IsBetterThanEquipped(Player* bot, ItemTemplate const* tmpl, uint8& slot)
    {
        // Сравниваем по item level с предметом в том же слоте.
        slot = GetEquipSlotFor(tmpl);
        if (slot == EQUIPMENT_SLOT_END)
            return false;

        if (Item const* equipped = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            if (ItemTemplate const* eqTmpl = equipped->GetTemplate())
                return tmpl->ItemLevel > eqTmpl->ItemLevel;

        return true; // слот пуст — предмет лучше.
    }

    // Ближайший живой дружественный NPC-торговец.
    Creature* FindNearbyVendor(Player* bot, float range)
    {
        std::list<Unit*> units;
        Acore::AnyUnitInObjectRangeCheck check(bot, range);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(bot, units, check);
        TypeContainerVisitor<Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck>, GridTypeMapContainer> visitor(searcher);
        Cell cell(Acore::ComputeCellCoord(bot->GetPositionX(), bot->GetPositionY()));
        cell.Visit(Acore::ComputeCellCoord(bot->GetPositionX(), bot->GetPositionY()), visitor, *bot->GetMap(), *bot, range);

        Creature* best = nullptr;
        float bestDist = range;

        for (Unit* unit : units)
        {
            Creature* creature = unit->ToCreature();
            if (!creature || !creature->IsAlive() || !creature->IsFriendlyTo(bot))
                continue;
            if (!(creature->GetNpcFlags() & UNIT_NPC_FLAG_VENDOR))
                continue;

            float dist = bot->GetDistance(creature);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = creature;
            }
        }

        return best;
    }

    // Сбыть накопленное торговцу.
    void SellStash(Player* bot, Creature* vendor)
    {
        ObjectGuid::LowType guid = bot->GetGUID().GetCounter();
        auto it = s_stash.find(guid);
        if (it == s_stash.end() || it->second.empty())
            return;

        uint32 total = 0;
        for (PendingLoot const& loot : it->second)
            total += loot.price;

        s_wallets[guid] += total;
        LOG_INFO("server.loading", ">> mod-ai-bots: бот {} продал {} предметов торговцу '{}' за {} меди.",
            bot->GetName(), uint32(it->second.size()), vendor->GetName(), total);
        it->second.clear();
    }
}

namespace AiBotsLoot
{
    void CollectFrom(Player* bot, Unit* victim)
    {
        if (!bot || !victim || !victim->IsCreature())
            return;

        ObjectGuid::LowType guid = bot->GetGUID().GetCounter();

        // Бонус золота от перков гильдии (Cash Flow, Bartering и др.).
        uint32 goldBonusPct = 100;
        if (bot->GetGuildId())
            goldBonusPct = AiBotsGuild::GetGoldBonusPct();

        // Золото пропорционально уровню жертвы.
        uint32 gold = (BaseGoldPerKill + victim->GetLevel() * urand(1, 3)) * goldBonusPct / 100;
        s_wallets[guid] += gold;

        // Часть золота — в казну гильдии (как Cash Flow).
        AiBotsGuild::DepositToVault(gold / 10);

        // Изредка с моба падает предмет.
        if (!roll_chance_i(ItemDropChance))
            return;

        ItemTemplate const* tmpl = RollRandomEquipment(bot);
        if (!tmpl)
            return;

        uint8 slot = EQUIPMENT_SLOT_END;
        if (IsBetterThanEquipped(bot, tmpl, slot))
        {
            // Лучшая подходящая вещь — надеваем (через штатный механизм).
            uint16 dest = 0;
            if (bot->CanEquipNewItem(NULL_SLOT, dest, tmpl->ItemId, true))
            {
                if (bot->EquipNewItem(dest, tmpl->ItemId, true))
                {
                    LOG_INFO("server.loading", ">> mod-ai-bots: бот {} надел '{}' (ilvl {}, слот {}) — лучше прежнего.",
                        bot->GetName(), tmpl->Name1, tmpl->ItemLevel, uint32(slot));
                    return;
                }
            }
        }

        // Продаём ближайшему торговцу; если его рядом нет — складываем в сумку.
        if (Creature* vendor = FindNearbyVendor(bot, VendorRange))
        {
            SellStash(bot, vendor);
            s_wallets[guid] += tmpl->SellPrice;
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} продал '{}' торговцу '{}' за {} меди.",
                bot->GetName(), tmpl->Name1, vendor->GetName(), tmpl->SellPrice);
            return;
        }

        std::vector<PendingLoot>& stash = s_stash[guid];
        stash.push_back({ tmpl->ItemId, tmpl->SellPrice });

        // Переполнилась сумка — самое старое всё равно уходит в золото.
        if (stash.size() > MaxStash)
        {
            s_wallets[guid] += stash.front().price;
            stash.erase(stash.begin());
        }
    }

    void AddCraftedItem(Player* bot, uint32 entry)
    {
        if (!bot || !entry)
            return;

        ItemTemplate const* tmpl = sObjectMgr->GetItemTemplate(entry);
        if (!tmpl)
            return;

        ObjectGuid::LowType guid = bot->GetGUID().GetCounter();
        uint32 price = tmpl->SellPrice ? tmpl->SellPrice : 1;

        if (Creature* vendor = FindNearbyVendor(bot, VendorRange))
        {
            SellStash(bot, vendor);
            s_wallets[guid] += price;
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} продал изготовленный предмет '{}' торговцу '{}' за {} меди.",
                bot->GetName(), tmpl->Name1, vendor->GetName(), price);
            return;
        }

        std::vector<PendingLoot>& stash = s_stash[guid];
        stash.push_back({ entry, price });
        if (stash.size() > MaxStash)
        {
            s_wallets[guid] += stash.front().price;
            stash.erase(stash.begin());
        }
    }

    uint32 GetGold(ObjectGuid::LowType botGuid)
    {
        auto it = s_wallets.find(botGuid);
        return it != s_wallets.end() ? it->second : 0;
    }
}
