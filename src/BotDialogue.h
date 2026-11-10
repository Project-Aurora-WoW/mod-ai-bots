/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotDialogue.h
 *
 * Реалистичные диалоги: ответы ботам на шёпот и реплики рядом,
 * короткие диалоги между ботами.
 */

#ifndef AI_BOTS_BOT_DIALOGUE_H
#define AI_BOTS_BOT_DIALOGUE_H

#include "Define.h"

#include <string>

class Player;

namespace AiBotsDialogue
{
    // Осмысленный ответ на сообщение игрока.
    std::string ReplyToPlayer(Player* bot, Player* sender, std::string const& message);

    // Игрок что-то сказал рядом — бот может ответить.
    void OnPlayerSaid(Player* speaker, uint32 type, std::string const& message);

    // Короткие диалоги между ботами.
    void Update(Player* bot, uint32 diff);
}

#endif // AI_BOTS_BOT_DIALOGUE_H
