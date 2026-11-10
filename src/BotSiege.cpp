/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotSiege.cpp
 *
 * Цели осад загружаются из таблицы mod_ai_bots_siege_targets
 * (настраиваются без пересборки); при пустой таблице — встроенный список.
 *
 * Цикл осады:
 *   1. Раз в Interval (90–240 мин) запускается осада случайного города;
 *   2. Отбирается отряд AttackerMin..AttackerMax автономных ботов
 *      противоположной фракции, они телепортируются к воротам города;
 *   3. Анонс в мир + боевые кличи;
 *   4. Боты сражаются с защитниками (штатный FindTarget/FindPvP);
 *   5. Через DurationMinutes — отступление: телепорт домой, анонс.
 */

#include "BotSiege.h"
#include "BotMgr.h"
#include "Config_Bots.h"
#include "WorldBots.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "World.h"
#include "WorldSessionMgr.h"
#include "Log.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "World.h"

#include <vector>

namespace
{
    // Fallback: встроенные цели, если таблица пуста/отсутствует.
    std::vector<BotSiegeTarget> const DefaultTargets =
    {
        { 0, -9050.0f,   440.0f, 94.0f, "Stormwind",   TEAM_ALLIANCE },
        { 0, -4981.0f,  -881.0f, 501.0f, "Ironforge",   TEAM_ALLIANCE },
        { 1,  1568.0f, -4297.0f,  61.0f, "Orgrimmar",   TEAM_HORDE },
        { 1,  1831.0f,   238.0f,  61.0f, "Undercity",   TEAM_HORDE },
    };

    std::vector<BotSiegeTarget> s_targets;
    bool s_targetsLoaded = false;

    std::vector<BotSiegeTarget> const& GetTargetsInternal()
    {
        if (!s_targetsLoaded)
        {
            s_targets.clear();

            if (QueryResult result = WorldDatabase.Query(
                "SELECT `mapId`, `x`, `y`, `z`, `name`, `faction` FROM `mod_ai_bots_siege_targets`"))
            {
                do
                {
                    Field* fields = result->Fetch();
                    s_targets.push_back(
                    {
                        fields[0].Get<uint32>(),
                        fields[1].Get<float>(),
                        fields[2].Get<float>(),
                        fields[3].Get<float>(),
                        fields[4].Get<std::string>(),
                        fields[5].Get<uint32>() == 1 ? TEAM_HORDE : TEAM_ALLIANCE
                    });
                } while (result->NextRow());

                LOG_INFO("server.loading", ">> mod-ai-bots: загружено {} целей осад из mod_ai_bots_siege_targets.", s_targets.size());
            }

            if (s_targets.empty())
            {
                LOG_WARN("server.loading", ">> mod-ai-bots: таблица mod_ai_bots_siege_targets пуста — используются встроенные цели.");
                s_targets = DefaultTargets;
            }

            s_targetsLoaded = true;
        }

        return s_targets;
    }

    enum SiegeState
    {
        SIEGE_IDLE = 0,
        SIEGE_RUNNING
    };

    SiegeState s_state = SIEGE_IDLE;
    uint32 s_nextSiegeMs = 0;      // до следующей осады
    uint32 s_siegeTimerMs = 0;     // длительность текущей осады
    uint32 s_stateLogMs = 0;       // диагностика состояния осады
    std::vector<ObjectGuid::LowType> s_attackers;

    void Announce(std::string const& text)
    {
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, text.c_str());
    }
}

namespace AiBotsSiege
{
    std::vector<BotSiegeTarget> const& GetTargets()
    {
        return GetTargetsInternal();
    }

    bool IsSiegeActive()
    {
        return s_state == SIEGE_RUNNING;
    }

    void Init()
    {
        if (!AiBotsConfig::IsSiegeEnabled())
            return;

        // Первая осада — через случайный интервал.
        s_nextSiegeMs = urand(AiBotsConfig::GetSiegeIntervalMin(), AiBotsConfig::GetSiegeIntervalMax()) * 60 * 1000;
        LOG_INFO("server.loading", ">> mod-ai-bots: первая осада города через {} минут.",
            s_nextSiegeMs / 60000);
    }

    bool StartSiege()
    {
        if (s_state != SIEGE_IDLE)
            return false;

        auto const& allBots = sBotMgr->GetAllBots();

        // Свободные (не в бою) живые боты.
        std::vector<Player*> available;
        for (auto const& pair : allBots)
        {
            if (Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(pair.first)))
                if (bot->IsInWorld() && bot->IsAlive() && !bot->IsInCombat())
                    available.push_back(bot);
        }

        // Автономные боты живут в WorldBots и в _allBots не попадают.
        sWorldBots->CollectOnlineBots(available);

        if (available.empty())
        {
            LOG_INFO("server.loading", ">> mod-ai-bots: осада отложена — автономных ботов в мире нет.");
            return false;
        }

        // Случайная цель и фракция атакующих.
        BotSiegeTarget const& target = GetTargetsInternal()[urand(0, GetTargetsInternal().size() - 1)];
        TeamId attackingTeam = (target.faction == TEAM_HORDE) ? TEAM_ALLIANCE : TEAM_HORDE;

        // Отбираем отряд нужной фракции на карте цели — смена карты телепортом ботов ломает.
        std::vector<Player*> squad;
        uint32 wanted = urand(AiBotsConfig::GetSiegeAttackerMin(), AiBotsConfig::GetSiegeAttackerMax());
        for (Player* bot : available)
        {
            if (squad.size() >= wanted)
                break;
            if (bot->GetTeamId() != attackingTeam)
                continue;
            if (bot->GetMapId() != target.mapId)
                continue;
            squad.push_back(bot);
        }

        if (squad.size() < 2)
        {
            LOG_INFO("server.loading", ">> mod-ai-bots: осада отложена — на карте {} в отряде {} ботов (нужно 2).",
                target.mapId, squad.size());
            return false;
        }

        // Телепорт к воротам города в пределах его карты.
        s_attackers.clear();
        for (Player* bot : squad)
        {
            float angle = frand(0.0f, 2.0f * float(M_PI));
            bot->NearTeleportTo(
                target.x + 5.0f * std::cos(angle),
                target.y + 5.0f * std::sin(angle),
                target.z, frand(0.0f, 2.0f * float(M_PI)));

            // У бота нет клиента, который подтвердит телепорт: завершаем сами,
            // иначе IsBeingTeleported() навсегда выключает AI.
            if (bot->IsBeingTeleportedNear())
            {
                bot->SetSemaphoreTeleportNear(0);
                WorldLocation const& dest = bot->GetTeleportDest();
                bot->UpdatePosition(dest, true);
                bot->SetFallInformation(GameTime::GetGameTime().count(), dest.GetPositionZ());
            }
            s_attackers.push_back(bot->GetGUID().GetCounter());
            sBotMgr->SetBotSiegeMode(bot->GetGUID().GetCounter(), true);
        }

        s_state = SIEGE_RUNNING;
        s_siegeTimerMs = AiBotsConfig::GetSiegeDurationMinutes() * 60 * 1000;

        std::string attackerName = (attackingTeam == TEAM_HORDE) ? "Horde" : "Alliance";
        Announce("[AI-Bots] The " + attackerName + " forces are storming " + target.name + "!");
        LOG_INFO("server.loading", ">> mod-ai-bots: осада '{}' начата, атакующих: {}.",
            target.name, s_attackers.size());

        s_stateLogMs = 0;
        LOG_INFO("server.loading", ">> mod-ai-bots: цель '{}' map {} pos ({:.0f},{:.0f},{:.0f}).",
            target.name, target.mapId, target.x, target.y, target.z);

        return true;
    }

    void EndSiege(bool retreat)
    {
        for (auto guid : s_attackers)
            sBotMgr->SetBotSiegeMode(guid, false);

        s_state = SIEGE_IDLE;
        s_attackers.clear();

        // Следующая осада через случайный интервал.
        s_nextSiegeMs = urand(AiBotsConfig::GetSiegeIntervalMin(), AiBotsConfig::GetSiegeIntervalMax()) * 60 * 1000;

        if (retreat)
            Announce("[AI-Bots] The siege forces have withdrawn. The city stands!");
    }

    void Update(uint32 diff)
    {
        if (!AiBotsConfig::IsSiegeEnabled())
            return;

        switch (s_state)
        {
            case SIEGE_IDLE:
            {
                // Отсчёт по реальному времени, а не по интервалу AI-тика.
                if (s_nextSiegeMs > diff)
                {
                    s_nextSiegeMs -= diff;
                    return;
                }
                s_nextSiegeMs = 0;
                if (!StartSiege())
                    s_nextSiegeMs = 30000; // повтор через 30 секунд
                break;
            }

            case SIEGE_RUNNING:
            {
                // Отсчёт длительности.
                if (s_siegeTimerMs > diff)
                    s_siegeTimerMs -= diff;
                else
                {
                    // Время вышло — отступление.
                    EndSiege(true);
                    break;
                }

                // Если атакующих в живых не осталось — осада провалена.
                uint32 aliveCount = 0;
                for (auto guid : s_attackers)
                {
                    if (Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(guid)))
                        if (bot->IsInWorld() && bot->IsAlive())
                            ++aliveCount;
                }

                if (aliveCount == 0)
                    EndSiege(true);

                // Диагностика: раз в 30 секунд — где боты и дерутся ли.
                s_stateLogMs += diff;
                if (s_stateLogMs >= 30000)
                {
                    s_stateLogMs = 0;
                    uint32 inCombat = 0;
                    for (auto guid : s_attackers)
                    {
                        Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
                        if (!bot)
                            continue;

                        bool alive = bot->IsInWorld() && bot->IsAlive();
                        bool fighting = alive && bot->IsInCombat();
                        if (fighting)
                            ++inCombat;

                        Unit* victim = bot->GetVictim();
                        LOG_INFO("server.loading",
                            ">> mod-ai-bots: осада — {}: alive={} combat={} map={} ({:.0f},{:.0f},{:.0f}) target={}.",
                            bot->GetName(), alive ? 1 : 0, fighting ? 1 : 0, bot->GetMapId(),
                            bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                            victim ? victim->GetName() : "none");
                    }

                    LOG_INFO("server.loading", ">> mod-ai-bots: осада — живых {}, в бою {}.",
                        aliveCount, inCombat);
                }

                break;
            }
        }
    }
}
