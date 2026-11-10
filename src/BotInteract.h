/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotInteract.h
 *
 * Взаимодействие ботов с игроками: ответ на шёпот, приём приглашений
 * в группу, подтверждение обмена и принятие дуэли.
 */

#ifndef AI_BOTS_BOT_INTERACT_H
#define AI_BOTS_BOT_INTERACT_H

#include "Define.h"

#include <string>

class Player;

namespace AiBotsInteract
{
    // Ответить игроку на шёпот (вызывается из хука чата).
    void OnPrivateChat(Player* sender, uint32 type, uint32 language, std::string& msg, Player* receiver);

    // Тик бота: приглашения в группу, трейд, дуэли.
    void Update(Player* bot);
}

#endif // AI_BOTS_BOT_INTERACT_H
