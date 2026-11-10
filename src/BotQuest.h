/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotQuest.h
 *
 * Квесты автономных ботов: рядом с квестодателем бот берёт посильный
 * квест на убийство существ, а после выполнения сдаёт его эндеру.
 */

#ifndef AI_BOTS_BOT_QUEST_H
#define AI_BOTS_BOT_QUEST_H

#include "Define.h"

class Player;

namespace AiBotsQuest
{
    // Один цикл квестовой логики (вызывается периодически из BotAI).
    void Update(Player* bot);

    // Является ли существо целью активного квеста бота (можно атаковать).
    bool IsRequiredTarget(Player* bot, uint32 entry);
}

#endif // AI_BOTS_BOT_QUEST_H
