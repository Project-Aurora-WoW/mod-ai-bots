/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotStats.h
 *
 * Статистика ботов как у игроков: убийства существ (PvE),
 * победы над игроками (PvP) и смерти.
 */

#ifndef AI_BOTS_BOT_STATS_H
#define AI_BOTS_BOT_STATS_H

#include "Define.h"

class Player;
class Unit;

namespace AiBotsStats
{
    // Бот добил существо или игрока.
    void OnCreatureKill(Player* bot, Unit* victim);

    // Бот погиб.
    void OnDeath(Player* bot);

    // Периодический сброс накопленных счётчиков в БД.
    void Update(Player* bot, uint32 diff);
}

#endif // AI_BOTS_BOT_STATS_H
