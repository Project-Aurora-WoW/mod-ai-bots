/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotStats.cpp
 *
 * Счётчики PvE/PvP/смертей копятся в памяти и раз в 30 секунд
 * дописываются в таблицу mod_ai_bots_stats (characters DB).
 */

#include "BotStats.h"

#include "DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "Unit.h"

#include <map>

namespace
{
    struct Delta
    {
        uint32 pvp = 0;
        uint32 pve = 0;
        uint32 deaths = 0;
    };

    std::map<ObjectGuid::LowType, Delta> s_deltas;
    std::map<ObjectGuid::LowType, uint32> s_timers;

    void Flush(ObjectGuid::LowType guid)
    {
        auto it = s_deltas.find(guid);
        if (it == s_deltas.end())
            return;

        Delta& delta = it->second;
        if (!delta.pvp && !delta.pve && !delta.deaths)
            return;

        CharacterDatabase.Execute(
            "INSERT INTO `mod_ai_bots_stats` (`botGuid`, `pvpKills`, `pveKills`, `deaths`) VALUES ({}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE `pvpKills` = `pvpKills` + VALUES(`pvpKills`), "
            "`pveKills` = `pveKills` + VALUES(`pveKills`), `deaths` = `deaths` + VALUES(`deaths`)",
            guid, delta.pvp, delta.pve, delta.deaths);

        delta.pvp = 0;
        delta.pve = 0;
        delta.deaths = 0;
    }
}

namespace AiBotsStats
{
    void OnCreatureKill(Player* bot, Unit* victim)
    {
        if (!bot || !victim)
            return;

        Delta& delta = s_deltas[bot->GetGUID().GetCounter()];
        if (victim->IsPlayer())
        {
            // PvP засчитываем только за игрока противоположной фракции.
            if (Player* victimPlayer = victim->ToPlayer())
                if (victimPlayer->GetTeamId() != bot->GetTeamId())
                    ++delta.pvp;
            return;
        }

        ++delta.pve;
    }

    void OnDeath(Player* bot)
    {
        if (!bot)
            return;

        ++s_deltas[bot->GetGUID().GetCounter()].deaths;
    }

    void Update(Player* bot, uint32 diff)
    {
        if (!bot)
            return;

        ObjectGuid::LowType guid = bot->GetGUID().GetCounter();
        uint32& timer = s_timers[guid];
        timer += diff;
        if (timer < 30000)
            return;

        timer = 0;
        Flush(guid);
    }
}
