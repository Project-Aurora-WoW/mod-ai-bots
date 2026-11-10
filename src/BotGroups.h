/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotGroups.h
 *
 * Группы автономных ботов: боты одной фракции рядом объединяются,
 * лидер координирует атаку, лекари лечат раненых товарищей.
 */

#ifndef AI_BOTS_BOT_GROUPS_H
#define AI_BOTS_BOT_GROUPS_H

#include "Define.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <cstdint>
#include <string>
#include <vector>

class Player;

namespace AiBotsGroups
{
    struct BotGroup
    {
        ObjectGuid::LowType leaderGuid;
        TeamId team;
        std::vector<ObjectGuid::LowType> members;
    };

    // Вызывается периодически: формирует группы и обеспечивает координацию.
    void Update(uint32 diff);

    // Группа бота (nullptr, если бот без группы).
    BotGroup const* GetGroupOf(ObjectGuid::LowType botGuid);

    // Лидер группы бота.
    Player* GetLeaderOf(ObjectGuid::LowType botGuid);
}

#endif // AI_BOTS_BOT_GROUPS_H
