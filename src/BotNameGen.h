/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotNameGen.h
 *
 * Процедурный генератор фэнтезийных имён ботов (слоговый конструктор).
 */

#ifndef AI_BOTS_BOT_NAME_GEN_H
#define AI_BOTS_BOT_NAME_GEN_H

#include "Define.h"

#include <string>

namespace AiBotsNameGen
{
    // Генерирует случайное имя: 2-3 слога, первая буква заглавная,
    // длина не более 12 символов (лимит имени персонажа).
    std::string Generate();
}

#endif // AI_BOTS_BOT_NAME_GEN_H
