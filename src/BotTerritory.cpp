/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotTerritory.cpp
 *
 * Территории хранятся в mod_ai_bots_territory (name, owner 0=ally 1=horde).
 * Раз в FlipMinutes одна случайная зона переходит к другой фракции
 * с объявлением в World чат. Владельца можно узнать командой .bot territory.
 */

#include "BotTerritory.h"
#include "Config_Bots.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "World.h"
#include "WorldSessionMgr.h"

#include <vector>

namespace
{
    std::vector<BotTerritoryZone> s_territories;
    uint32 s_flipTimer = 0;

    std::string const FactionName(TeamId team)
    {
        return team == TEAM_HORDE ? "Horde" : "Alliance";
    }
}

namespace AiBotsTerritory
{
    void Init()
    {
        if (!AiBotsConfig::IsTerritoryEnabled())
            return;

        s_territories.clear();

        QueryResult result = WorldDatabase.Query("SELECT `id`, `name`, `owner` FROM `mod_ai_bots_territory`");
        if (!result)
        {
            LOG_WARN("server.loading", ">> mod-ai-bots: таблица mod_ai_bots_territory пуста или отсутствует — территории выключены.");
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            s_territories.push_back(
            {
                fields[0].Get<uint32>(),
                fields[1].Get<std::string>(),
                fields[2].Get<uint32>() == 1 ? TEAM_HORDE : TEAM_ALLIANCE
            });
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> mod-ai-bots: загружено {} территорий.", s_territories.size());
    }

    void Update(uint32 diff)
    {
        if (!AiBotsConfig::IsTerritoryEnabled() || s_territories.empty())
            return;

        s_flipTimer += diff;
        if (s_flipTimer < AiBotsConfig::GetTerritoryFlipMinutes() * 60 * 1000)
            return;
        s_flipTimer = 0;

        // Случайная зона меняет владельца.
        BotTerritoryZone& zone = s_territories[urand(0, s_territories.size() - 1)];
        zone.owner = (zone.owner == TEAM_HORDE) ? TEAM_ALLIANCE : TEAM_HORDE;

        // Сохраняем в БД.
        WorldDatabase.DirectExecute(
            "UPDATE `mod_ai_bots_territory` SET `owner` = {} WHERE `id` = {}",
            zone.owner == TEAM_HORDE ? 1 : 0, zone.id);

        // Объявление в мир.
        std::string message = "[AI-Bots] Territory '" + zone.name +
            "' has been captured by the " + FactionName(zone.owner) + "!";
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, message.c_str());

        LOG_INFO("server.loading", ">> mod-ai-bots: территория '{}' захвачена фракцией {}.",
            zone.name, FactionName(zone.owner));
    }

    std::vector<BotTerritoryZone> const& GetAll()
    {
        return s_territories;
    }

    TeamId GetOwner(std::string const& name)
    {
        for (auto const& zone : s_territories)
            if (zone.name == name)
                return zone.owner;
        return TEAM_NEUTRAL;
    }
}
