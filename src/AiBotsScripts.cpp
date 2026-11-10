/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/AiBotsScripts.cpp
 *
 * Точка входа модуля: команды .bot, автоспавн при логине,
 * автономные боты в мире (WorldBots), обновление AI в WorldScript-тикере,
 * загрузка конфига.
 */

#include "BotAI.h"
#include "BotMgr.h"
#include "Config_Bots.h"
#include "WorldBots.h"
#include "BotGroups.h"
#include "BotSocial.h"
#include "BotTerritory.h"
#include "BotVendors.h"
#include "BotLoot.h"
#include "BotGuild.h"
#include "BotPets.h"
#include "BotSiege.h"
#include "BotParty.h"
#include "BotInteract.h"
#include "BotStats.h"
#include "BotDialogue.h"
#include "BotProgression.h"
#include "BotConsoleAppender.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Chat.h"
#include "CommandScript.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldScript.h"

using namespace Acore::ChatCommands;

namespace
{
    // Регистрируем UTF-8-аппендер до инициализации логов (статическая
    // инициализация выполняется раньше main, где читается конфиг логов).
    struct BotConsoleAppenderRegistrar
    {
        BotConsoleAppenderRegistrar()
        {
            Log::instance()->RegisterAppender<BotConsoleAppender>();
        }
    } s_botConsoleAppenderRegistrar;
}

class AiBotsCommandScript : public CommandScript
{
public:
    AiBotsCommandScript() : CommandScript("AiBotsCommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable botCommandTable =
        {
            { "add",       HandleBotAdd,       SEC_PLAYER, Console::No },
            { "remove",    HandleBotRemove,    SEC_PLAYER, Console::No },
            { "list",      HandleBotList,      SEC_PLAYER, Console::No },
            { "info",      HandleBotInfo,      SEC_PLAYER, Console::Yes },
            { "follow",    HandleBotFollow,    SEC_PLAYER, Console::No },
            { "party",     HandleBotParty,     SEC_PLAYER, Console::No },
            { "stats",     HandleBotStats,     SEC_PLAYER, Console::No },
            { "shop",      HandleBotShop,      SEC_PLAYER, Console::No },
            { "buy",       HandleBotBuy,       SEC_PLAYER, Console::No },
            { "gold",      HandleBotGold,      SEC_PLAYER, Console::No },
            { "pet",       HandleBotPet,       SEC_PLAYER, Console::No },
            { "groups",    HandleBotGroups,    SEC_PLAYER, Console::No },
            { "territory", HandleBotTerritory, SEC_PLAYER, Console::No },
            { "guild",     HandleBotGuild,     SEC_PLAYER, Console::No },
            { "siege",     HandleBotSiege,     SEC_PLAYER, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "bot", botCommandTable },
        };

        return commandTable;
    }

    static bool HandleBotAdd(ChatHandler* handler, Optional<std::string> args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        if (!AiBotsConfig::IsEnabled())
        {
            handler->PSendSysMessage("[AI-Bot] Модуль выключен (AiBots.Enable = 0).");
            return true;
        }

        uint8 race = 0, cls = 0;
        if (args && !args->empty())
        {
            // формат: .bot add <race>:<class>, например .bot add 1:1
            auto pos = args->find(':');
            if (pos != std::string::npos)
            {
                try
                {
                    race = static_cast<uint8>(std::stoi(args->substr(0, pos)));
                    cls  = static_cast<uint8>(std::stoi(args->substr(pos + 1)));
                }
                catch (std::exception const&)
                {
                    handler->PSendSysMessage("[AI-Bot] Неверный формат. Использование: .bot add [race:class]");
                    return true;
                }
            }
        }

        sBotMgr->CreateBotForPlayer(player, race, cls);
        return true;
    }

    static bool HandleBotRemove(ChatHandler* handler, std::string const& botName)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        if (!AiBotsConfig::IsEnabled())
        {
            handler->PSendSysMessage("[AI-Bot] Модуль выключен (AiBots.Enable = 0).");
            return true;
        }

        if (botName.empty())
        {
            sBotMgr->RemoveAllBots(player);
            handler->PSendSysMessage("[AI-Bot] Все боты удалены.");
        }
        else
            sBotMgr->RemoveBot(player, botName);

        return true;
    }

    static bool HandleBotList(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        auto const& bots = sBotMgr->GetBotsOfOwner(player->GetGUID().GetCounter());
        if (bots.empty())
        {
            handler->PSendSysMessage("[AI-Bot] У вас нет ботов. Создайте: .bot add [race:class]");
            return true;
        }

        handler->PSendSysMessage("[AI-Bot] Ваши боты:");
        for (auto const& bot : bots)
            handler->PSendSysMessage("  - {} (раса {}, класс {}, уровень {}, в мире: {})",
                bot.name, bot.race, bot.cls, bot.level, bot.inWorld ? "да" : "нет");

        return true;
    }

    static char const* ClassNameRu(uint8 cls)
    {
        switch (cls)
        {
            case 1:  return "Воин";
            case 2:  return "Паладин";
            case 3:  return "Охотник";
            case 4:  return "Разбойник";
            case 5:  return "Жрец";
            case 6:  return "Рыцарь смерти";
            case 7:  return "Шаман";
            case 8:  return "Маг";
            case 9:  return "Чернокнижник";
            case 11: return "Друид";
            default: return "неизвестно";
        }
    }

    static char const* RaceNameRu(uint8 race)
    {
        switch (race)
        {
            case 1:  return "Человек";
            case 2:  return "Орк";
            case 3:  return "Дворф";
            case 4:  return "Ночной эльф";
            case 5:  return "Нежить";
            case 6:  return "Таурен";
            case 7:  return "Гном";
            case 8:  return "Тролль";
            case 10: return "Кровавый эльф";
            case 11: return "Дреней";
            default: return "неизвестно";
        }
    }

    static char const* MapNameRu(uint32 mapId)
    {
        switch (mapId)
        {
            case 0:   return "Восточные королевства";
            case 1:   return "Калимдор";
            case 530: return "Запределье";
            case 571: return "Нордскол";
            default:  return "неизвестная карта";
        }
    }

    static char const* ZoneNameRu(uint32 zoneId)
    {
        switch (zoneId)
        {
            case 1:    return "Дун Морог";
            case 12:   return "Элвиннский лес";
            case 14:   return "Дуротар";
            case 17:   return "Степи";
            case 38:   return "Лок Модан";
            case 40:   return "Западный край";
            case 44:   return "Красногорье";
            case 85:   return "Тирисфальские леса";
            case 130:  return "Серебряный бор";
            case 141:  return "Тельдрассил";
            case 148:  return "Тёмные берега";
            case 215:  return "Мулгор";
            case 331:  return "Ясеневый лес";
            case 406:  return "Каменный Клык";
            case 1519: return "Штормград";
            case 1637: return "Оргриммар";
            default:   return nullptr;
        }
    }

    static bool HandleBotInfo(ChatHandler* handler)
    {
        struct BotRow
        {
            ObjectGuid::LowType guid = 0;
            std::string name = "неизвестно";
            uint8 level = 0;
            uint8 race = 0;
            uint8 cls = 0;
            bool inWorld = false;
            bool known = false;
        };

        std::map<ObjectGuid::LowType, BotRow> rows;

        for (auto const& pair : sBotMgr->GetAllBots())
        {
            BotData const& data = pair.second;
            BotRow& row = rows[data.guid];
            row.guid = data.guid;
            row.name = data.name;
            row.level = data.level;
            row.race = data.race;
            row.cls = data.cls;
            row.inWorld = data.inWorld;
            row.known = true;
        }

        for (ObjectGuid::LowType guid : sWorldBots->GetBotGuids())
        {
            BotRow& row = rows[guid];
            row.guid = guid;

            if (Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(guid)))
            {
                row.name = bot->GetName();
                row.level = bot->GetLevel();
                row.race = bot->getRace();
                row.cls = bot->getClass();
                row.inWorld = true;
                row.known = true;
                continue;
            }

            if (row.known)
                continue;

            if (QueryResult result = CharacterDatabase.Query("SELECT name, level, race, class FROM characters WHERE guid = {}", guid))
            {
                Field* fields = result->Fetch();
                row.name = fields[0].Get<std::string>();
                row.level = fields[1].Get<uint8>();
                row.race = fields[2].Get<uint8>();
                row.cls = fields[3].Get<uint8>();
                row.known = true;
            }
        }

        if (rows.empty())
        {
            if (handler->GetSession())
                handler->PSendSysMessage("[Боты] В модуле нет ботов.");
            else
                LOG_INFO("server.loading", "[Боты] В модуле нет ботов.");
            return true;
        }

        uint32 online = 0, alive = 0, dead = 0, combat = 0, levelSum = 0, knownCount = 0;
        for (auto const& pair : rows)
        {
            BotRow const& row = pair.second;
            if (row.level)
            {
                levelSum += row.level;
                ++knownCount;
            }

            if (!row.inWorld)
                continue;

            ++online;
            Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(row.guid));
            if (!bot)
                continue;

            if (bot->IsAlive())
            {
                ++alive;
                if (bot->IsInCombat())
                    ++combat;
            }
            else
                ++dead;
        }

        uint32 const avgLevel = knownCount ? levelSum / knownCount : 0;

        if (handler->GetSession())
        {
            handler->PSendSysMessage("[Боты] Всего: {}, в мире: {}, живы: {}, мертвы: {}, в бою: {}, средний уровень: {}.",
                uint32(rows.size()), online, alive, dead, combat, avgLevel);
            handler->PSendSysMessage("{:<16} {:>3} {:<9} {:<14} {:<14} {:<10} {:>6} {:<10}  {}",
                "Имя", "Ур", "Фракция", "Класс", "Раса", "Состояние", "Здоровье", "PvP/ПвЕ/См", "Локация");
        }
        else
        {
            LOG_INFO("server.loading", "[Боты] Всего: {}, в мире: {}, живы: {}, мертвы: {}, в бою: {}, средний уровень: {}.",
                uint32(rows.size()), online, alive, dead, combat, avgLevel);
            LOG_INFO("server.loading", "{:<16} {:>3} {:<9} {:<14} {:<14} {:<10} {:>6} {:<10}  {}",
                "Имя", "Ур", "Фракция", "Класс", "Раса", "Состояние", "Здоровье", "PvP/ПвЕ/См", "Локация");
        }

        for (auto const& pair : rows)
        {
            BotRow const& row = pair.second;
            Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(row.guid));

            bool const hordeRace = row.race == 2 || row.race == 5 || row.race == 6 || row.race == 8 || row.race == 10;
            std::string faction = bot ? (bot->GetTeamId() == TEAM_ALLIANCE ? "Альянс" : "Орда")
                                      : (hordeRace ? "Орда" : "Альянс");
            std::string state = row.inWorld ? "в мире" : "не в мире";
            std::string health = "—";
            std::string location = "—";

            if (bot)
            {
                state = bot->IsAlive() ? (bot->IsInCombat() ? "в бою" : "жив") : "мёртв";
                health = std::to_string(uint32(bot->GetHealthPct())) + "%";

                char const* zone = ZoneNameRu(bot->GetZoneId());
                location = MapNameRu(bot->GetMapId());
                location += zone ? " / " + std::string(zone)
                                 : " / зона " + std::to_string(bot->GetZoneId());
            }

            std::string const levelText = row.known ? std::to_string(row.level) : std::string("?");

            uint32 pvpKills = 0, pveKills = 0, deaths = 0;
            if (QueryResult stats = CharacterDatabase.Query("SELECT pvpKills, pveKills, deaths FROM mod_ai_bots_stats WHERE botGuid = {}", row.guid))
            {
                Field* sf = stats->Fetch();
                pvpKills = sf[0].Get<uint32>();
                pveKills = sf[1].Get<uint32>();
                deaths = sf[2].Get<uint32>();
            }
            std::string const statsText = std::to_string(pvpKills) + "/" + std::to_string(pveKills) + "/" + std::to_string(deaths);

            if (handler->GetSession())
                handler->PSendSysMessage("  {:<16} {:>3} {:<9} {:<14} {:<14} {:<10} {:>6} {:<10}  {}",
                    row.name, levelText, faction, ClassNameRu(row.cls), RaceNameRu(row.race), state, health, statsText, location);
            else
                LOG_INFO("server.loading", "  {:<16} {:>3} {:<9} {:<14} {:<14} {:<10} {:>6} {:<10}  {}",
                    row.name, levelText, faction, ClassNameRu(row.cls), RaceNameRu(row.race), state, health, statsText, location);
        }

        return true;
    }

    static bool HandleBotFollow(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        auto const& bots = sBotMgr->GetBotsOfOwner(player->GetGUID().GetCounter());
        for (auto const& botData : bots)
        {
            if (Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(botData.guid)))
            {
                bot->GetMotionMaster()->MoveFollow(player, AiBotsConfig::GetFollowDistance(), 0.0f);
                handler->PSendSysMessage("[AI-Bot] '{}' следует за вами.", botData.name);
            }
        }

        return true;
    }

    static bool HandleBotParty(ChatHandler* handler, Optional<std::string> args)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        std::string sub = args ? *args : "";
        if (sub == "leave" || sub == "remove")
        {
            if (AiBotsParty::RemoveOwnedBots(player))
                handler->PSendSysMessage("[AI-Bot] Боты покинули отряд.");
            else
                handler->PSendSysMessage("[AI-Bot] В отряде нет ваших ботов.");
            return true;
        }

        if (AiBotsParty::AddOwnedBots(player))
            handler->PSendSysMessage("[AI-Bot] Боты вступили в отряд (в группе: {}).",
                AiBotsParty::CountOwnedBotsInGroup(player));
        else
            handler->PSendSysMessage("[AI-Bot] Добавить не удалось: боты офлайн или уже в группе.");

        return true;
    }

    static bool HandleBotStats(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        auto const& bots = sBotMgr->GetBotsOfOwner(player->GetGUID().GetCounter());
        if (bots.empty())
        {
            handler->PSendSysMessage("[AI-Bot] У вас нет ботов.");
            return true;
        }

        handler->PSendSysMessage("[AI-Bot] Статистика ботов:");
        for (auto const& data : bots)
        {
            uint32 pvp = 0, pve = 0, deaths = 0;
            if (QueryResult result = CharacterDatabase.Query(
                "SELECT `pvpKills`, `pveKills`, `deaths` FROM `mod_ai_bots_stats` WHERE `botGuid` = {}", data.guid))
            {
                Field* fields = result->Fetch();
                pvp = fields[0].Get<uint32>();
                pve = fields[1].Get<uint32>();
                deaths = fields[2].Get<uint32>();
            }

            std::string extra;
            if (Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(data.guid)))
                extra = " (честь: " + std::to_string(bot->GetHonorPoints()) +
                        ", убийств: " + std::to_string(bot->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS)) + ")";

            handler->PSendSysMessage("  {}: PvE {}, PvP {}, смертей {}{}",
                data.name, pve, pvp, deaths, extra);
        }

        return true;
    }

    // Найти ближайшего бота-торговца в радиусе 15 ярдов.
    static Player* FindNearbyVendor(Player* player)
    {
        // Ищем среди зарегистрированных торговцев, а не только в _allBots:
        // боты-торговцы входят в мир отдельной сессией и в этом реестре не живут.
        return AiBotsVendors::FindNearest(player, 15.0f);
    }

    static bool HandleBotShop(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player || !AiBotsConfig::IsVendorsEnabled())
            return false;

        Player* vendor = FindNearbyVendor(player);
        if (!vendor)
        {
            handler->PSendSysMessage("[AI-Bot] Рядом нет торговца (радиус 15 ярдов).");
            return true;
        }

        auto const* stock = AiBotsVendors::GetStock(vendor->GetGUID().GetCounter());
        if (!stock || stock->empty())
        {
            handler->PSendSysMessage("[AI-Bot] У торговца '{}' нет товаров.", vendor->GetName());
            return true;
        }

        handler->PSendSysMessage("[AI-Bot] Товары торговца '{}':", vendor->GetName());
        uint32 index = 1;
        for (auto const& item : *stock)
            handler->PSendSysMessage("  {}. itemId={} цена={}м запас={}",
                index++, item.itemId, item.price, item.stock == 0 ? "бесконечный" : std::to_string(item.stock));

        handler->PSendSysMessage("Покупка: .bot buy <номер>");
        return true;
    }

    static bool HandleBotBuy(ChatHandler* handler, uint32 itemIndex)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player || !AiBotsConfig::IsVendorsEnabled())
            return false;

        Player* vendor = FindNearbyVendor(player);
        if (!vendor)
        {
            handler->PSendSysMessage("[AI-Bot] Рядом нет торговца (радиус 15 ярдов).");
            return true;
        }

        // Индексы для игрока начинаются с 1.
        if (itemIndex == 0)
        {
            handler->PSendSysMessage("[AI-Bot] Укажите номер товара (см. .bot shop).");
            return true;
        }

        std::string error = AiBotsVendors::Buy(player, vendor->GetGUID().GetCounter(), itemIndex - 1);
        if (error.empty())
            handler->PSendSysMessage("[AI-Bot] Покупка совершена у '{}'.", vendor->GetName());
        else
            handler->PSendSysMessage("[AI-Bot] {}.", error);

        return true;
    }

    // Команда .bot gold — показать виртуальные кошельки ботов игрока.
    // Лут собирается автоматически (BotLoot), но не имел пользовательского интерфейса.
    static bool HandleBotGold(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        auto const& bots = sBotMgr->GetBotsOfOwner(player->GetGUID().GetCounter());
        if (bots.empty())
        {
            handler->PSendSysMessage("[AI-Bot] У вас нет ботов. Создайте: .bot add [race:class]");
            return true;
        }

        handler->PSendSysMessage("[AI-Bot] Кошельки ваших ботов:");
        for (auto const& bot : bots)
        {
            uint32 copper = AiBotsLoot::GetGold(bot.guid);
            handler->PSendSysMessage("  - {}: {}з {}с {}м",
                bot.name, copper / 10000, (copper / 100) % 100, copper % 100);
        }

        return true;
    }

    // Команда .bot pet — призвать питомца ботам-охотникам/чернокнижникам.
    // Подсистема BotPets была полностью автоматической без ручного управления.
    static bool HandleBotPet(ChatHandler* handler)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
            return false;

        if (!AiBotsConfig::IsPetsEnabled())
        {
            handler->PSendSysMessage("[AI-Bot] Питомцы выключены (AiBots.Pets.Enable = 0).");
            return true;
        }

        uint32 summoned = 0;
        for (auto const& bot : sBotMgr->GetBotsOfOwner(player->GetGUID().GetCounter()))
        {
            if (!bot.inWorld || !AiBotsPets::ClassHasPet(bot.cls))
                continue;

            Player* botPlayer = sBotMgr->GetBotPlayer(bot.guid);
            if (!botPlayer || !botPlayer->IsInWorld())
                continue;

            if (AiBotsPets::SummonPetFor(botPlayer))
                ++summoned;
        }

        if (summoned > 0)
            handler->PSendSysMessage("[AI-Bot] Призвано питомцев: {}.", summoned);
        else
            handler->PSendSysMessage("[AI-Bot] Нет ботов в мире с классом питомца (охотник/чернокнижник).");

        return true;
    }

    // Команда .bot groups — список групп автономных ботов.
    // API групп (BotGroups) был доступен только внутренним подсистемам.
    static bool HandleBotGroups(ChatHandler* handler)
    {
        if (!AiBotsConfig::IsGroupsEnabled())
        {
            handler->PSendSysMessage("[AI-Bot] Группы ботов выключены.");
            return true;
        }

        // Собираем уникальные группы: группируем ботов по guid лидера.
        // В группах есть и боты владельцев, и автономные мировые.
        std::vector<ObjectGuid::LowType> botGuids;
        for (auto const& pair : sBotMgr->GetAllBots())
            botGuids.push_back(pair.first);
        for (ObjectGuid::LowType guid : sWorldBots->GetBotGuids())
            botGuids.push_back(guid);

        std::map<ObjectGuid::LowType, std::vector<std::string>> groupsByLeader;
        for (ObjectGuid::LowType guid : botGuids)
        {
            auto const* group = AiBotsGroups::GetGroupOf(guid);
            if (!group)
                continue;

            Player* member = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
            if (!member || !member->IsInWorld())
                continue;

            groupsByLeader[group->leaderGuid].push_back(member->GetName());
        }

        if (groupsByLeader.empty())
        {
            handler->PSendSysMessage("[AI-Bot] Сейчас нет активных групп ботов.");
            return true;
        }

        handler->PSendSysMessage("[AI-Bot] Группы ботов:");
        for (auto const& entry : groupsByLeader)
        {
            Player* leader = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(entry.first));
            std::string leaderName = leader && leader->IsInWorld()
                ? leader->GetName() : "<нет в мире>";

            handler->PSendSysMessage("  Лидер '{}', участников {}:",
                leaderName, entry.second.size());
            for (auto const& memberName : entry.second)
                if (memberName != leaderName)
                    handler->PSendSysMessage("    - {}", memberName);
        }

        return true;
    }

    static bool HandleBotTerritory(ChatHandler* handler)
    {
        if (!AiBotsConfig::IsTerritoryEnabled())
        {
            handler->PSendSysMessage("[AI-Bot] Система территорий выключена.");
            return true;
        }

        handler->PSendSysMessage("[AI-Bot] Территории:");
        for (auto const& zone : AiBotsTerritory::GetAll())
            handler->PSendSysMessage("  - {}: {}", zone.name, zone.owner == TEAM_HORDE ? "Орда" : "Альянс");

        return true;
    }

    static bool HandleBotGuild(ChatHandler* handler)
    {
        if (!AiBotsConfig::IsGuildEnabled())
        {
            handler->PSendSysMessage("[AI-Bot] Гильдия ботов выключена.");
            return true;
        }

        // Гильдии отдельные для каждой фракции — выводим обе.
        TeamId const teams[] = { TEAM_ALLIANCE, TEAM_HORDE };
        for (TeamId team : teams)
        {
            handler->PSendSysMessage("[AI-Bot] Гильдия '{}' ({}):",
                AiBotsGuild::GetGuildNameFor(team),
                team == TEAM_HORDE ? "Орда" : "Альянс");
            handler->PSendSysMessage("  Уровень: {}/{}",
                AiBotsGuild::GetLevel(), AiBotsConfig::GetGuildMaxLevel());
            handler->PSendSysMessage("  Опыт: {}/{}",
                AiBotsGuild::GetXp(), AiBotsGuild::GetXpForNextLevel());
            uint32 vault = AiBotsGuild::GetVault();
            handler->PSendSysMessage("  Казна: {}з {}с {}м",
                vault / 10000, (vault / 100) % 100, vault % 100);
            handler->PSendSysMessage("  Состав: {} участников", AiBotsGuild::GetMemberCount());

            uint32 xp = AiBotsGuild::GetXp();
            uint32 xpNext = AiBotsGuild::GetXpForNextLevel();
            uint32 pct = xpNext ? (xp * 100) / xpNext : 0;
            handler->PSendSysMessage("  Прогресс: {}%", pct);
        }

        return true;
    }

    static bool HandleBotSiege(ChatHandler* handler)
    {
        if (!AiBotsConfig::IsSiegeEnabled())
        {
            handler->PSendSysMessage("[AI-Bot] Осады выключены.");
            return true;
        }

        if (AiBotsSiege::IsSiegeActive())
            handler->PSendSysMessage("[AI-Bot] Осада идёт прямо сейчас!");
        else
            handler->PSendSysMessage("[AI-Bot] Осада не активна. Следующая — по расписанию ({}-{} мин).",
                AiBotsConfig::GetSiegeIntervalMin(), AiBotsConfig::GetSiegeIntervalMax());

        handler->PSendSysMessage("[AI-Bot] Цели осад:");
        for (auto const& target : AiBotsSiege::GetTargets())
            handler->PSendSysMessage("  - {} ({})", target.name,
                target.faction == TEAM_HORDE ? "Орда" : "Альянс");

        return true;
    }
};

class AiBotsPlayerScript : public PlayerScript
{
public:
    AiBotsPlayerScript() : PlayerScript("AiBotsPlayerScript") {}

    void OnPlayerLogin(Player* player) override
    {
        sBotMgr->HandlePlayerLogin(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        sBotMgr->HandlePlayerLogout(player);
    }

    void OnPlayerMapChanged(Player* player) override
    {
        AiBotsParty::OnPlayerMapChanged(player);
    }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg, Player* receiver) override
    {
        AiBotsInteract::OnPrivateChat(player, type, language, msg, receiver);
        return true;
    }

    bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language, std::string& msg) override
    {
        AiBotsDialogue::OnPlayerSaid(player, type, msg);
        return true;
    }

    void OnPlayerJustDied(Player* player) override
    {
        if (sBotMgr->IsBot(player->GetGUID().GetCounter()))
            AiBotsStats::OnDeath(player);
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 /*xpSource*/) override
    {
        if (sBotMgr->IsBot(player->GetGUID().GetCounter()))
            AiBotsProgression::ScaleXp(player, amount);
    }

    void OnPlayerLevelChanged(Player* player, uint8 oldLevel) override
    {
        if (sBotMgr->IsBot(player->GetGUID().GetCounter()))
            AiBotsProgression::OnLevelChanged(player, oldLevel);
    }
};

class AiBotsWorldScript : public WorldScript
{
public:
    AiBotsWorldScript() : WorldScript("AiBotsWorldScript") {}

    void OnBeforeConfigLoad(bool reload) override
    {
        AiBotsConfig::Load();
    }

    void OnStartup() override
    {
        // Проверка кодировки: кириллица, «кавычки» и символы вне CP866.
        LOG_INFO("server.loading", ">> mod-ai-bots: UTF-8-вывод — проверка: Привет, мир! «Ёжик», ツ ✓");

        // Автономные боты, торговцы, территории и осады при старте мира.
        LOG_INFO("server.loading", ">> mod-ai-bots: прогрессия {} (кап {}, опыт x{}, авто-прокачка {} каждые {} с).",
            AiBotsConfig::IsProgressionEnabled() ? "включена" : "выключена",
            AiBotsConfig::GetProgressionMaxLevel(), AiBotsConfig::GetProgressionXpRate(),
            AiBotsConfig::IsProgressionAutoLevelEnabled() ? "включена" : "выключена",
            AiBotsConfig::GetProgressionAutoLevelIntervalSec());
        // Торговцы — первыми: WorldBots исключает их из случайного пула.
        AiBotsVendors::SpawnAll();
        sWorldBots->SpawnAll();
        AiBotsTerritory::Init();
        AiBotsSiege::Init();
    }

    void OnUpdate(uint32 diff) override
    {
        sBotMgr->Update(diff);
        AiBotsParty::Update(diff);
        sWorldBots->Update(diff);
        AiBotsGroups::Update(diff);
        AiBotsSocial::Update(diff);
        AiBotsTerritory::Update(diff);
        AiBotsVendors::Update(diff);
        AiBotsGuild::Update(diff);
        AiBotsPets::Update(diff);
        AiBotsSiege::Update(diff);
    }
};

void AddAiBotsScripts()
{
    new AiBotsCommandScript();
    new AiBotsPlayerScript();
    new AiBotsWorldScript();
}

