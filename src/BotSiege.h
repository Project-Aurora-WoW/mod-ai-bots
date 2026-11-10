/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotSiege.h
 *
 * Осады городов: отряд ботов эпизодически штурмует вражескую столицу.
 * Атакующие собираются, идут к городу, сражаются с защитниками
 * (боты и NPC), завершают бой победой или отступлением.
 */

#ifndef AI_BOTS_BOT_SIEGE_H
#define AI_BOTS_BOT_SIEGE_H

#include "Define.h"
#include "SharedDefines.h"

#include <cstdint>
#include <string>
#include <vector>

class Player;

struct BotSiegeTarget
{
    uint32 mapId;
    float x;
    float y;
    float z;
    std::string name;
    TeamId faction; // фракция-владелец города
};

namespace AiBotsSiege
{
    // Инициализация (расписание первой осады).
    void Init();

    // Периодическое обновление: запуск осад, логика боя, завершение.
    void Update(uint32 diff);

    // Активная осада сейчас?
    bool IsSiegeActive();

    // Список городов-целей (для будущих расширений).
    std::vector<BotSiegeTarget> const& GetTargets();
}

#endif // AI_BOTS_BOT_SIEGE_H
