/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotChat.h
 *
 * Общение ботов в мировом канале и гильдейском чате.
 */

#ifndef AI_BOTS_BOT_CHAT_H
#define AI_BOTS_BOT_CHAT_H

#include "Define.h"

class Player;

namespace AiBotsChat
{
    void Update(Player* bot, uint32 diff);
}

#endif // AI_BOTS_BOT_CHAT_H
