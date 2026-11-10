/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotPersonality.h
 *
 * Личности ботов: у каждой — свой набор реплик (англ.).
 * Личность назначается детерминированно по guid, чтобы бот
 * сохранял характер между входами в мир.
 */

#ifndef AI_BOTS_BOT_PERSONALITY_H
#define AI_BOTS_BOT_PERSONALITY_H

#include "Define.h"
#include "ObjectGuid.h"

#include <cstdint>
#include <string>
#include <vector>

struct BotPersonality
{
    std::string name;
    std::vector<std::string> battleCries;
    std::vector<std::string> victory;
    std::vector<std::string> idle;
    std::vector<std::string> flee;
    std::vector<std::string> heal;
};

namespace AiBotsPersonality
{
    std::vector<BotPersonality> const& GetAll();
    BotPersonality const& GetForGuid(ObjectGuid::LowType guid);
}

#endif // AI_BOTS_BOT_PERSONALITY_H
