/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotLoot.h
 *
 * Лут: боты собирают золото и предметы с убитых мобов.
 * Оружие/броня экипируются, всё остальное «продаётся» в золото.
 */

#ifndef AI_BOTS_BOT_LOOT_H
#define AI_BOTS_BOT_LOOT_H

#include "Define.h"
#include "ObjectGuid.h"

#include <cstdint>

class Player;
class Unit;

namespace AiBotsLoot
{
    // Бот собирает добычу с убитой цели.
    void CollectFrom(Player* bot, Unit* victim);

    // Изготовленный профессией предмет: продать торговцу или отложить.
    void AddCraftedItem(Player* bot, uint32 entry);

    // Текущий запас золота бота (виртуальный кошелёк).
    uint32 GetGold(ObjectGuid::LowType botGuid);
}

#endif // AI_BOTS_BOT_LOOT_H
