/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotGear.cpp
 *
 * Экипировка ботов под уровень: броня по типу класса и оружие подбираются
 * из item_template по требуемому уровню, а не фиксированным набором 60-го.
 * Статические наборы используются только как запасной вариант.
 */

#include "BotGear.h"

#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "Player.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    // Запасные наборы (уровень 60) — если под уровень ничего не нашлось.
    AiBotsGear::BotGearSet const plateSet = { 16955, 21330, 21652, 16954, 21333, 21623, 19137, 21618, 12584, 0 };
    AiBotsGear::BotGearSet const mailSet  = { 16939, 21376, 16942, 21626, 21365, 21624, 19393, 16935, 18827, 18833 };
    AiBotsGear::BotGearSet const leatherSet = { 16900, 21354, 21680, 16901, 21355, 21617, 16903, 16904, 18838, 0 };
    AiBotsGear::BotGearSet const clothSet = { 16914, 21335, 22756, 19385, 21338, 21339, 19388, 16918, 18873, 0 };

    uint8 BestArmorSubclass(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:  return 4; // plate
            case CLASS_HUNTER:
            case CLASS_SHAMAN:   return 3; // mail
            case CLASS_ROGUE:
            case CLASS_DRUID:    return 2; // leather
            default:             return 1; // cloth
        }
    }

    std::string LevelWindow(uint8 level)
    {
        return " AND `RequiredLevel` <= " + std::to_string(level) +
               " AND `RequiredLevel` >= " + std::to_string(level > 12 ? level - 12 : 1);
    }

    std::vector<uint32> QueryItems(std::string const& sql)
    {
        std::vector<uint32> items;
        if (QueryResult result = WorldDatabase.Query(sql.c_str()))
        {
            do
            {
                items.push_back(result->Fetch()[0].Get<uint32>());
            } while (result->NextRow());
        }
        return items;
    }

    // Броня: от лучшего доступного типа класса вниз до ткани.
    std::vector<uint32> CollectArmor(uint8 cls, uint8 invType, uint8 level)
    {
        uint32 classMask = 1u << (cls - 1);
        uint8 best = BestArmorSubclass(cls);

        for (int sub = int(best); sub >= 1; --sub)
        {
            std::string base = std::string("SELECT `entry` FROM `item_template` WHERE `class` = 4") +
                " AND `subclass` = " + std::to_string(sub) +
                " AND `InventoryType` IN (" + std::to_string(invType) + (invType == 5 ? ",20" : "") + ")" +
                " AND (`AllowableClass` = 0 OR (`AllowableClass` & " + std::to_string(classMask) + ") <> 0)";

            std::vector<uint32> items = QueryItems(base + LevelWindow(level) +
                " ORDER BY `ItemLevel` DESC LIMIT 8");
            if (!items.empty())
                return items;

            items = QueryItems(base +
                " AND `RequiredLevel` <= " + std::to_string(level) +
                " ORDER BY `RequiredLevel` DESC, `ItemLevel` DESC LIMIT 8");
            if (!items.empty())
                return items;
        }

        return {};
    }

    // Оружие ближнего боя.
    std::vector<uint32> CollectWeapon(uint8 cls, uint8 level)
    {
        uint32 classMask = 1u << (cls - 1);
        std::string base = std::string("SELECT `entry` FROM `item_template` WHERE `class` = 2") +
            " AND `InventoryType` IN (13,17) AND `subclass` IN (0,1,4,5,7,8,10,15)" +
            " AND (`AllowableClass` = 0 OR (`AllowableClass` & " + std::to_string(classMask) + ") <> 0)";

        std::vector<uint32> items = QueryItems(base + LevelWindow(level) +
            " ORDER BY `ItemLevel` DESC LIMIT 8");
        if (!items.empty())
            return items;

        return QueryItems(base + " AND `RequiredLevel` <= " + std::to_string(level) +
            " ORDER BY `RequiredLevel` DESC, `ItemLevel` DESC LIMIT 8");
    }

    // Дальний бой — только охотникам.
    std::vector<uint32> CollectRanged(uint8 cls, uint8 level)
    {
        if (cls != CLASS_HUNTER)
            return {};

        uint32 classMask = 1u << (cls - 1);
        std::string base = std::string("SELECT `entry` FROM `item_template` WHERE `class` = 2") +
            " AND `InventoryType` IN (15,26) AND `subclass` IN (2,3,6)" +
            " AND (`AllowableClass` = 0 OR (`AllowableClass` & " + std::to_string(classMask) + ") <> 0)";

        std::vector<uint32> items = QueryItems(base + LevelWindow(level) +
            " ORDER BY `ItemLevel` DESC LIMIT 8");
        if (!items.empty())
            return items;

        return QueryItems(base + " AND `RequiredLevel` <= " + std::to_string(level) +
            " ORDER BY `RequiredLevel` DESC, `ItemLevel` DESC LIMIT 8");
    }

    void EquipSlot(Player* bot, uint8 slot, std::vector<uint32> const& candidates, uint32 fallbackEntry)
    {
        if (bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            return;

        uint32 entry = 0;
        if (!candidates.empty())
            entry = candidates[urand(0, std::min<size_t>(candidates.size(), 4) - 1)];
        else
            entry = fallbackEntry;

        if (!entry)
            return;

        if (Item* item = Item::CreateItem(entry, 1, bot))
            bot->EquipItem(uint16((INVENTORY_SLOT_BAG_0 << 8) | slot), item, true);
    }
}

namespace AiBotsGear
{
    BotGearSet const& GetGearForClass(uint8 cls)
    {
        switch (cls)
        {
            case CLASS_WARRIOR:
            case CLASS_PALADIN:  return plateSet;
            case CLASS_HUNTER:
            case CLASS_SHAMAN:   return mailSet;
            case CLASS_ROGUE:
            case CLASS_DRUID:    return leatherSet;
            case CLASS_PRIEST:
            case CLASS_MAGE:
            case CLASS_WARLOCK:  return clothSet;
            default:             return clothSet;
        }
    }

    void RandomizeAppearance(uint8& skin, uint8& face, uint8& hairStyle,
                             uint8& hairColor, uint8& facialFeature, uint8& tattoos)
    {
        skin         = urand(0, 6);
        face         = urand(0, 5);
        hairStyle    = urand(0, 10);
        hairColor    = urand(0, 8);
        facialFeature= urand(0, 5);
        tattoos      = 0;
    }

    void EquipGearFor(Player* bot)
    {
        if (!bot)
            return;

        uint8 const level = bot->GetLevel();
        uint8 const cls = bot->getClass();
        BotGearSet const& fallback = GetGearForClass(cls);

        struct ArmorSlot
        {
            uint8 slot;
            uint8 invType;
            uint32 fallbackEntry;
        };

        ArmorSlot const armor[] =
        {
            { EQUIPMENT_SLOT_HEAD,      1,  fallback.head },
            { EQUIPMENT_SLOT_SHOULDERS, 3,  fallback.shoulders },
            { EQUIPMENT_SLOT_CHEST,     5,  fallback.chest },
            { EQUIPMENT_SLOT_WAIST,     6,  fallback.waist },
            { EQUIPMENT_SLOT_LEGS,      7,  fallback.legs },
            { EQUIPMENT_SLOT_FEET,      8,  fallback.feet },
            { EQUIPMENT_SLOT_WRISTS,    9,  fallback.wrists },
            { EQUIPMENT_SLOT_HANDS,     10, fallback.hands },
        };

        for (ArmorSlot const& a : armor)
            EquipSlot(bot, a.slot, CollectArmor(cls, a.invType, level), a.fallbackEntry);

        EquipSlot(bot, EQUIPMENT_SLOT_MAINHAND, CollectWeapon(cls, level), fallback.mainHand);

        if (cls == CLASS_HUNTER)
            EquipSlot(bot, EQUIPMENT_SLOT_RANGED, CollectRanged(cls, level), fallback.ranged);

        uint32 suitable = 0;
        uint32 total = 0;
        for (ArmorSlot const& a : armor)
        {
            if (Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, a.slot))
            {
                ++total;
                if (item->GetTemplate() && item->GetTemplate()->RequiredLevel <= level)
                    ++suitable;
            }
        }

        Item* mainHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        uint32 mainReq = mainHand && mainHand->GetTemplate() ? mainHand->GetTemplate()->RequiredLevel : 0;

        LOG_INFO("server.loading", ">> mod-ai-bots: бот {} (уровень {}): броня {}/{} по уровню, оружие {} (треб. {}).",
            bot->GetName(), uint32(level), suitable, total, mainHand ? mainHand->GetEntry() : 0, mainReq);
    }
}
