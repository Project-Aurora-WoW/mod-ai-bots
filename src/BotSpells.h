/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotSpells.h
 *
 * Базовые заклинания по классам (боевые, лечебные, баффы)
 * и фразы чата ботов (англ.).
 */

#ifndef AI_BOTS_BOT_SPELLS_H
#define AI_BOTS_BOT_SPELLS_H

#include "Define.h"
#include "SharedDefines.h"

#include <cstdint>
#include <string>
#include <vector>

namespace AiBotsSpells
{
    // Боевые способности по классам.
    std::vector<uint32> const& GetSpellsForClass(uint8 cls);

    // Лечебные способности по классам (пусто, если класс не умеет лечить).
    std::vector<uint32> const& GetHealSpellsForClass(uint8 cls);

    // Баффы по классам (кастуются до боя, на себя и союзников).
    std::vector<uint32> const& GetBuffSpellsForClass(uint8 cls);

    // Английские фразы чата.
    std::vector<std::string> const& GetBattleCries();
    std::vector<std::string> const& GetVictoryPhrases();
    std::vector<std::string> const& GetIdlePhrases();
    std::vector<std::string> const& GetDeathPhrases();
    std::vector<std::string> const& GetFleePhrases();
    std::vector<std::string> const& GetHealPhrases();
}

#endif // AI_BOTS_BOT_SPELLS_H
