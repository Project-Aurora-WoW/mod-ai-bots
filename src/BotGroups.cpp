/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotGroups.cpp
 *
 * Формирование групп: каждые N секунд боты без группы одной фракции
 * рядом (30 ярд.) объединяются до MaxSize. Лекари группы лечат
 * раненых товарищей, лидер задаёт общую цель.
 */

#include "BotGroups.h"
#include "BotMgr.h"
#include "BotSpells.h"
#include "Config_Bots.h"
#include "WorldBots.h"

#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Unit.h"

#include <algorithm>
#include <map>
#include <vector>

namespace
{
    std::vector<AiBotsGroups::BotGroup> s_groups;
    uint32 s_regroupTimer = 0;

    uint32 const RegroupIntervalMs = 20000;
    float const GroupJoinRange = 30.0f;
    float const HealAllyRange = 20.0f;

    bool IsHealerClass(uint8 cls)
    {
        return cls == CLASS_PRIEST || cls == CLASS_SHAMAN ||
               cls == CLASS_PALADIN || cls == CLASS_DRUID;
    }
}

namespace AiBotsGroups
{
    BotGroup const* GetGroupOf(ObjectGuid::LowType botGuid)
    {
        for (auto const& group : s_groups)
            for (auto member : group.members)
                if (member == botGuid)
                    return &group;
        return nullptr;
    }

    Player* GetLeaderOf(ObjectGuid::LowType botGuid)
    {
        BotGroup const* group = GetGroupOf(botGuid);
        if (!group)
            return nullptr;
        return ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(group->leaderGuid));
    }

    void Update(uint32 diff)
    {
        if (!AiBotsConfig::IsWorldBotsEnabled() || !AiBotsConfig::IsGroupsEnabled())
            return;

        s_regroupTimer += diff;
        if (s_regroupTimer < RegroupIntervalMs)
            return;
        s_regroupTimer = 0;

        // Пересборка: удаляем группы, где осталось меньше двух живых членов.
        s_groups.erase(std::remove_if(s_groups.begin(), s_groups.end(),
            [](BotGroup const& g)
            {
                uint32 alive = 0;
                for (auto member : g.members)
                    if (Player* p = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(member)))
                        if (p->IsInWorld() && p->IsAlive())
                            ++alive;
                return alive < 2;
            }), s_groups.end());

        auto const& allBots = sBotMgr->GetAllBots();
        uint32 maxSize = AiBotsConfig::GetGroupMaxSize();
        if (maxSize < 2)
            maxSize = 2;

        // Собираем живых ботов в мире без группы: и ботов владельцев,
        // и автономных мировых (они в _allBots не попадают).
        std::vector<Player*> free;

        auto consider = [&free](Player* p)
        {
            if (!p || !p->IsInWorld() || !p->IsAlive() || p->GetGroup())
                return;
            if (GetGroupOf(p->GetGUID().GetCounter()))
                return;
            free.push_back(p);
        };

        for (auto const& pair : allBots)
            if (Player* p = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(pair.first)))
                consider(p);

        std::vector<Player*> worldBots;
        sWorldBots->CollectOnlineBots(worldBots);
        for (Player* p : worldBots)
            consider(p);

        // Жадное объединение: первый свободный собирает рядом стоящих той же фракции.
        for (Player* initiator : free)
        {
            if (!initiator || GetGroupOf(initiator->GetGUID().GetCounter()))
                continue;

            BotGroup group;
            group.leaderGuid = initiator->GetGUID().GetCounter();
            group.team = initiator->GetTeamId();
            group.members.push_back(group.leaderGuid);

            for (Player* candidate : free)
            {
                if (group.members.size() >= maxSize)
                    break;
                if (candidate == initiator || GetGroupOf(candidate->GetGUID().GetCounter()))
                    continue;
                if (candidate->GetMapId() != initiator->GetMapId())
                    continue;
                if (candidate->GetTeamId() != group.team)
                    continue;
                if (initiator->GetDistance(candidate) > GroupJoinRange)
                    continue;

                group.members.push_back(candidate->GetGUID().GetCounter());
            }

            if (group.members.size() >= 2)
                s_groups.push_back(group);
        }

        // Лечение раненых товарищей по группе (лекари).
        for (auto const& group : s_groups)
        {
            for (auto memberGuid : group.members)
            {
                Player* healer = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(memberGuid));
                if (!healer || !healer->IsAlive() || healer->IsInCombat())
                    continue;
                if (!IsHealerClass(healer->getClass()))
                    continue;

                // Ищем самого раненого товарища в радиусе.
                Player* wounded = nullptr;
                float lowestPct = 100.0f;
                for (auto allyGuid : group.members)
                {
                    if (allyGuid == memberGuid)
                        continue;
                    Player* ally = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(allyGuid));
                    if (!ally || !ally->IsInWorld() || !ally->IsAlive())
                        continue;
                    if (healer->GetDistance(ally) > HealAllyRange)
                        continue;
                    if (ally->GetHealthPct() < lowestPct && ally->GetHealthPct() < 70.0f)
                    {
                        lowestPct = ally->GetHealthPct();
                        wounded = ally;
                    }
                }

                if (!wounded)
                    continue;

                // Первый доступный лечебный спелл класса.
                for (uint32 spellId : AiBotsSpells::GetHealSpellsForClass(healer->getClass()))
                {
                    if (!healer->HasSpell(spellId) || healer->HasSpellCooldown(spellId))
                        continue;
                    SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
                    if (!info || healer->GetDistance(wounded) > info->GetMaxRange())
                        continue;
                    healer->CastSpell(wounded, spellId, true);
                    break;
                }
            }
        }
    }
}
