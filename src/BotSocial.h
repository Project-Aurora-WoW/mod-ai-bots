/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotSocial.h
 *
 * Социальная подсистема: реплики при встречах, короткие диалоги
 * между ботами одной фракции, дуэли ботов враждебных фракций.
 */

#ifndef AI_BOTS_BOT_SOCIAL_H
#define AI_BOTS_BOT_SOCIAL_H

#include "Define.h"
#include "SharedDefines.h"

#include <cstdint>

class Player;

namespace AiBotsSocial
{
    // Вызывается периодически: обрабатывает встречи ботов рядом.
    void Update(uint32 diff);

    // Реплика приветствия (своя фракция) или угрозы (враждебная).
    void Greet(Player* bot, Player* other);

    // Попытка начать дуэль между двумя ботами (шанс из конфига).
    bool TryStartDuel(Player* bot, Player* other);
}

#endif // AI_BOTS_BOT_SOCIAL_H
