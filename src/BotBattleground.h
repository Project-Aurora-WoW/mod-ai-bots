/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotBattleground.h
 *
 * Участие автономных ботов в полях сражений: очередь, приём приглашения,
 * завершение телепорта. Бой в BG идёт обычным PvP-AI.
 */

#ifndef AI_BOTS_BOT_BATTLEGROUND_H
#define AI_BOTS_BOT_BATTLEGROUND_H

#include "Define.h"

class Player;

namespace AiBotsBattleground
{
    void Update(Player* bot, uint32 diff);
}

#endif // AI_BOTS_BOT_BATTLEGROUND_H
