/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotProgression.h
 *
 * Прогрессия ботов: множитель опыта, кап уровня, авто-прокачка,
 * обновление экипировки и навыков при повышении уровня.
 */

#ifndef AI_BOTS_BOT_PROGRESSION_H
#define AI_BOTS_BOT_PROGRESSION_H

#include "Define.h"

class Player;

namespace AiBotsProgression
{
    void ScaleXp(Player* bot, uint32& amount);
    void OnLevelChanged(Player* bot, uint8 oldLevel);
}

#endif // AI_BOTS_BOT_PROGRESSION_H
