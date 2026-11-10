/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotSkills.h
 *
 * Обучение ботов: классовые способности по уровню и одна основная
 * профессия с рецептами; периодическое «использование» профессии.
 */

#ifndef AI_BOTS_BOT_SKILLS_H
#define AI_BOTS_BOT_SKILLS_H

#include "Define.h"

class Player;

namespace AiBotsSkills
{
    // Выучить классовые способности, доступные на текущем уровне.
    void LearnForLevel(Player* bot);

    // Освоить профессию и её рецепты.
    void LearnProfessions(Player* bot);

    // Периодически «работать» профессией (создавать предметы).
    void UpdateProfessions(Player* bot, uint32 diff);
}

#endif // AI_BOTS_BOT_SKILLS_H
