/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotGather.h
 *
 * Сбор травы и руды с нод в мире.
 */

#ifndef AI_BOTS_BOT_GATHER_H
#define AI_BOTS_BOT_GATHER_H

#include "Define.h"

class Player;

namespace AiBotsGather
{
    void Update(Player* bot, uint32 diff);
}

#endif // AI_BOTS_BOT_GATHER_H
