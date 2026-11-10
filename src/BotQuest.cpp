/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotQuest.cpp
 *
 * Бот берёт рядом с квестодателем только те квесты, которые реально может
 * закрыть сам: цели — убийство существ, без предметов и игровых объектов.
 * Зачёт убийств делает ядро (KillRewarder -> Player::KilledMonster), бот —
 * обычный игрок. Сдача — у NPC-эндера в радиусе, награда — первый вариант.
 */

#include "BotQuest.h"

#include "BotAI.h"
#include "BotMgr.h"
#include "Cell.h"
#include "CellImpl.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"

#include <cmath>
#include <list>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace
{
    float const AcceptRange = 40.0f;     // дистанция взятия квеста у NPC
    float const TurnInRange = 50.0f;     // дистанция сдачи квеста эндеру
    float const HuntRange = 600.0f;      // поиск целей квеста и эндера
    uint32 const QuestMoveCooldownMs = 15000; // сколько не патрулировать, идя к цели

    // Диагностика: чтобы не спамить лог каждые 10 секунд.
    std::set<std::pair<ObjectGuid::LowType, uint32>> s_reportedComplete;
    std::set<std::pair<ObjectGuid::LowType, uint32>> s_reportedBlocked;
    // Диагностика прогресса: последний известный счётчик целей.
    std::map<std::pair<ObjectGuid::LowType, uint32>, uint32> s_lastProgress;
    uint8 const MaxActiveQuests = 1;     // бот ведёт не больше одного квеста
    uint8 const MaxQuestLevelGap = 3;    // допуск по уровню квеста

    bool QuestSuitable(Player* bot, Quest const* quest);    // Собираем существ рядом с ботом (NPC-квестодатели/эндеры).
    std::list<Unit*> CollectNearbyUnits(Player* bot, float range)
    {
        std::list<Unit*> units;
        Acore::AnyUnitInObjectRangeCheck check(bot, range);
        Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(bot, units, check);
        TypeContainerVisitor<Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck>, GridTypeMapContainer> visitor(searcher);
        Cell cell(Acore::ComputeCellCoord(bot->GetPositionX(), bot->GetPositionY()));
        cell.Visit(Acore::ComputeCellCoord(bot->GetPositionX(), bot->GetPositionY()), visitor, *bot->GetMap(), *bot, range);
        return units;
    }

    // Квест посилен боту: только убийства существ, без предметов и ГО.
    bool IsKillQuest(Quest const* quest)
    {
        bool hasKill = false;

        for (uint8 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
        {
            int32 required = quest->RequiredNpcOrGo[i];
            if (required == 0)
                continue;

            if (required < 0)              // цель — игровой объект
                return false;
            if (quest->RequiredNpcOrGoCount[i] == 0)
                return false;

            hasKill = true;
        }

        for (uint8 i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
            if (quest->RequiredItemId[i] != 0)
                return false;

        return hasKill;
    }

    bool HasFreeQuestSlot(Player* bot)
    {
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            if (bot->GetQuestSlotQuestId(slot) == 0)
                return true;
        return false;
    }

    uint8 CountActiveQuests(Player* bot)
    {
        uint8 count = 0;
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            if (bot->GetQuestSlotQuestId(slot) != 0)
                ++count;
        return count;
    }

    bool IsRequiredKillTarget(Player* bot, uint32 entry)
    {
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 questId = bot->GetQuestSlotQuestId(slot);
            if (!questId || bot->GetQuestStatus(questId) != QUEST_STATUS_INCOMPLETE)
                continue;

            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest)
                continue;

            for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
                if (quest->RequiredNpcOrGo[j] > 0 && uint32(quest->RequiredNpcOrGo[j]) == entry &&
                    quest->RequiredNpcOrGoCount[j] > 0)
                    return true;
        }

        return false;
    }

    // Все ли цели-убийства есть рядом: далёкий квест брать бессмысленно.
    bool HasAllObjectivesNearby(Player* bot, Quest const* quest, std::list<Unit*> const& nearby)
    {
        for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            int32 required = quest->RequiredNpcOrGo[j];
            if (required <= 0)
                continue;

            bool found = false;
            for (Unit* unit : nearby)
            {
                Creature* creature = unit->ToCreature();
                if (!creature || !creature->IsAlive() || !bot->IsValidAttackTarget(creature))
                    continue;
                if (creature->GetEntry() == uint32(required))
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                return false;
        }

        return true;
    }

    // Есть ли рядом хоть одна цель квеста: если да — квест брать уже можно.
    bool HasAnyObjectiveNearby(Player* bot, Quest const* quest, std::list<Unit*> const& nearby)
    {
        for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            int32 required = quest->RequiredNpcOrGo[j];
            if (required <= 0)
                continue;

            for (Unit* unit : nearby)
            {
                Creature* creature = unit->ToCreature();
                if (!creature || !creature->IsAlive() || !bot->IsValidAttackTarget(creature))
                    continue;
                if (creature->GetEntry() == uint32(required))
                    return true;
            }
        }

        return false;
    }

    // Точки спавна целей квеста из world.creature (кэш по карте и entry).
    struct ObjectiveSpawn
    {
        float x;
        float y;
        float z;
    };

    std::map<std::pair<uint32, uint32>, std::vector<ObjectiveSpawn>> s_spawnCache;

    std::vector<ObjectiveSpawn> const& GetObjectiveSpawns(uint32 mapId, uint32 entry)
    {
        auto const key = std::make_pair(mapId, entry);
        auto it = s_spawnCache.find(key);
        if (it != s_spawnCache.end())
            return it->second;

        std::vector<ObjectiveSpawn>& spawns = s_spawnCache[key];
        if (QueryResult result = WorldDatabase.Query(
            "SELECT `position_x`, `position_y`, `position_z` FROM `creature` "
            "WHERE `map` = {} AND `id` = {}",
            mapId, entry))
        {
            do
            {
                Field* fields = result->Fetch();
                spawns.push_back({ fields[0].Get<float>(), fields[1].Get<float>(), fields[2].Get<float>() });
            } while (result->NextRow());
        }

        return spawns;
    }

    // Есть ли у квеста цели, которые существуют в мире (хотя бы точки спавна).
    bool QuestHasSpawns(Player* bot, Quest const* quest)
    {
        for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            int32 required = quest->RequiredNpcOrGo[j];
            if (required <= 0)
                continue;
            if (!GetObjectiveSpawns(bot->GetMapId(), uint32(required)).empty())
                return true;
        }

        return false;
    }

    // Существует ли цель квеста в мире на любой карте. После рестарта ботов
    // переселяют на другую карту — квест от этого не становится недостижимым.
    bool QuestExistsAnywhere(Quest const* quest)
    {
        static std::map<uint32, bool> s_exists;

        for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            int32 required = quest->RequiredNpcOrGo[j];
            if (required <= 0)
                continue;

            uint32 const entry = uint32(required);
            auto it = s_exists.find(entry);
            if (it != s_exists.end())
            {
                if (it->second)
                    return true;
                continue;
            }

            QueryResult result = WorldDatabase.Query("SELECT 1 FROM `creature` WHERE `id` = {} LIMIT 1", entry);
            bool const exists = bool(result);
            s_exists[entry] = exists;

            if (exists)
                return true;
        }

        return false;
    }

    // Ближайшая точка спавна цели квеста на текущей карте бота.
    bool FindNearestObjectiveSpawn(Player* bot, Quest const* quest, float& x, float& y, float& z)
    {
        bool found = false;
        float best = 1.0e9f;

        for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            int32 required = quest->RequiredNpcOrGo[j];
            if (required <= 0)
                continue;

            for (ObjectiveSpawn const& spawn : GetObjectiveSpawns(bot->GetMapId(), uint32(required)))
            {
                float dx = spawn.x - bot->GetPositionX();
                float dy = spawn.y - bot->GetPositionY();
                float dz = spawn.z - bot->GetPositionZ();
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

                if (dist < best)
                {
                    best = dist;
                    x = spawn.x;
                    y = spawn.y;
                    z = spawn.z;
                    found = true;
                }
            }
        }

        return found;
    }

    // Марш к точке спавна: мобы могли быть убиты — бот дождётся респавна рядом.
    void IssueQuestMoveToPoint(Player* bot, float x, float y, float z)
    {
        struct PointIntent { float x; float y; float z; };
        static std::map<ObjectGuid::LowType, PointIntent> s_intents;
        PointIntent& intent = s_intents[bot->GetGUID().GetCounter()];

        bool const samePoint = std::fabs(intent.x - x) < 1.0f &&
                               std::fabs(intent.y - y) < 1.0f &&
                               std::fabs(intent.z - z) < 1.0f;
        bool const stillMoving = bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE
            && !bot->movespline->Finalized();

        if (!samePoint || !stillMoving)
        {
            bot->GetMotionMaster()->MovePoint(3, x, y, z, FORCED_MOVEMENT_NONE, 0.0f, 0.0f, false);

            float dx = x - bot->GetPositionX();
            float dy = y - bot->GetPositionY();
            float dz = z - bot->GetPositionZ();
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} идёт к точке спавна цели ({:.0f} м).",
                bot->GetName(), std::sqrt(dx * dx + dy * dy + dz * dz));
        }

        intent.x = x;
        intent.y = y;
        intent.z = z;

        if (BotAI* ai = sBotMgr->GetBotAI(bot->GetGUID().GetCounter()))
            ai->SetQuestMoveCooldown(QuestMoveCooldownMs);
    }

    void IssueQuestMove(Player* bot, Creature* target)
    {
        // Не переставляем точку, если уже идём к этой же цели: иначе сплайн
        // перезапускается каждые 3 секунды и бот топчется на месте.
        struct MoveIntent { ObjectGuid target; };
        static std::map<ObjectGuid::LowType, MoveIntent> s_intents;
        MoveIntent& intent = s_intents[bot->GetGUID().GetCounter()];
        bool const sameTarget = (intent.target == target->GetGUID());
        bool const stillMoving = bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE
            && !bot->movespline->Finalized();
        if (!sameTarget || !stillMoving)
            // Прямая линия: pathfinding обрезает путь в 40–50 м от цели,
            // и бот топчется на месте вместо того, чтобы подойти.
            bot->GetMotionMaster()->MovePoint(3, target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(),
                FORCED_MOVEMENT_NONE, 0.0f, 0.0f, false);
        intent.target = target->GetGUID();

        if (BotAI* ai = sBotMgr->GetBotAI(bot->GetGUID().GetCounter()))
            ai->SetQuestMoveCooldown(QuestMoveCooldownMs);

        // Диагностика: раз в 10 секунд показываем, куда и как идёт бот.
        struct LastMoveState { float x; float y; float z; uint32 when; };
        static std::map<ObjectGuid::LowType, LastMoveState> s_lastMoveLog;
        uint32 now = uint32(GameTime::GetGameTime().count());
        auto it = s_lastMoveLog.find(bot->GetGUID().GetCounter());
        if (it == s_lastMoveLog.end() || now - it->second.when >= 10)
        {
            float moved = 0.0f;
            if (it != s_lastMoveLog.end())
            {
                float dx = bot->GetPositionX() - it->second.x;
                float dy = bot->GetPositionY() - it->second.y;
                float dz = bot->GetPositionZ() - it->second.z;
                moved = std::sqrt(dx * dx + dy * dy + dz * dz);
            }
            s_lastMoveLog[bot->GetGUID().GetCounter()] = { bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), now };
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} идёт к {} (guid {}, {:.0f} м), смещение {:.1f} м, motion {}, spline {}/{}, cast {}, combat {}.",
                bot->GetName(), target->GetName(), target->GetGUID().GetCounter(), bot->GetDistance(target), moved,
                uint32(bot->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                bot->movespline->timePassed(), bot->movespline->Duration(),
                bot->HasUnitState(UNIT_STATE_CASTING), bot->IsInCombat());
        }
    }

    // Если цель квеста далеко — ведём бота к ней.
    bool MoveToQuestTarget(Player* bot)
    {
        Creature* best = nullptr;
        float bestDist = HuntRange;

        for (Unit* unit : CollectNearbyUnits(bot, HuntRange))
        {
            Creature* creature = unit->ToCreature();
            if (!creature || !creature->IsAlive() || !bot->IsValidAttackTarget(creature))
                continue;
            if (!IsRequiredKillTarget(bot, creature->GetEntry()))
                continue;

            float dist = bot->GetDistance(creature);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = creature;
            }
        }

        // Живая цель далеко — идём к ней.
        if (best && bestDist > 25.0f)
        {
            IssueQuestMove(bot, best);
            return true;
        }

        // Живых целей рядом нет (мобы мертвы или ещё не респавнулись) —
        // идём к точке их спавна из БД и ждём респавна на месте.
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 questId = bot->GetQuestSlotQuestId(slot);
            if (!questId || bot->GetQuestStatus(questId) != QUEST_STATUS_INCOMPLETE)
                continue;

            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest)
                continue;

            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (!FindNearestObjectiveSpawn(bot, quest, x, y, z))
                continue;

            float dx = x - bot->GetPositionX();
            float dy = y - bot->GetPositionY();
            float dz = z - bot->GetPositionZ();
            float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (dist <= 25.0f || dist > 3000.0f)
                continue;

            IssueQuestMoveToPoint(bot, x, y, z);
            return true;
        }

        return false;
    }

    // Если подходящий квестодатель далеко — ведём бота к нему.
    bool MoveToQuestGiver(Player* bot)
    {
        Creature* best = nullptr;
        float bestDist = HuntRange;

        std::list<Unit*> nearby = CollectNearbyUnits(bot, HuntRange);

        for (Unit* unit : nearby)
        {
            Creature* creature = unit->ToCreature();
            if (!creature || !creature->IsAlive() || !creature->IsFriendlyTo(bot))
                continue;

            bool hasSuitable = false;
            auto bounds = sObjectMgr->GetCreatureQuestRelationBounds(creature->GetEntry());
            for (auto it = bounds.first; it != bounds.second && !hasSuitable; ++it)
            {
                Quest const* quest = sObjectMgr->GetQuestTemplate(it->second);
                // Квестодатель нужен только если его цели есть рядом: иначе
                // бот идёт к нему впустую и топчется у него на месте.
                hasSuitable = QuestSuitable(bot, quest) && HasAnyObjectiveNearby(bot, quest, nearby);
            }

            if (!hasSuitable)
                continue;

            float dist = bot->GetDistance(creature);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = creature;
            }
        }

        if (!best || bestDist <= AcceptRange)
            return false;

        IssueQuestMove(bot, best);
        return true;
    }

    // Если эндер далеко — ведём бота к нему.
    bool MoveToQuestEnder(Player* bot, uint32 questId)
    {
        Creature* best = nullptr;
        float bestDist = HuntRange;

        for (Unit* unit : CollectNearbyUnits(bot, HuntRange))
        {
            Creature* creature = unit->ToCreature();
            if (!creature || !creature->IsAlive() || !creature->IsFriendlyTo(bot))
                continue;

            auto bounds = sObjectMgr->GetCreatureQuestInvolvedRelationBounds(creature->GetEntry());
            bool found = false;
            for (auto it = bounds.first; it != bounds.second; ++it)
            {
                if (it->second == questId)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                continue;

            float dist = bot->GetDistance(creature);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = creature;
            }
        }

        if (!best || bestDist <= TurnInRange)
            return false;

        IssueQuestMove(bot, best);
        return true;
    }

    bool HasIncompleteQuest(Player* bot)
    {
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 questId = bot->GetQuestSlotQuestId(slot);
            if (questId && bot->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
                return true;
        }

        return false;
    }

    bool QuestSuitable(Player* bot, Quest const* quest)
    {
        if (!quest || !IsKillQuest(quest))
            return false;

        if (bot->GetQuestStatus(quest->GetQuestId()) != QUEST_STATUS_NONE)
            return false;

        int32 questLevel = quest->GetQuestLevel();
        if (questLevel > int32(bot->GetLevel()) + MaxQuestLevelGap)
            return false;

        return bot->CanTakeQuest(quest, false);
    }

    void ReportProgress(Player* bot)
    {
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 questId = bot->GetQuestSlotQuestId(slot);
            if (!questId || bot->GetQuestStatus(questId) != QUEST_STATUS_INCOMPLETE)
                continue;

            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest)
                continue;

            QuestStatusData& data = bot->getQuestStatusMap()[questId];
            uint32 total = 0;
            for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
                total += data.CreatureOrGOCount[j];

            auto const key = std::make_pair(bot->GetGUID().GetCounter(), questId);
            auto it = s_lastProgress.find(key);
            if (it == s_lastProgress.end() || it->second != total)
            {
                s_lastProgress[key] = total;
                LOG_INFO("server.loading", ">> mod-ai-bots: бот {} прогресс квеста '{}' ({}): {}.",
                    bot->GetName(), quest->GetTitle(), questId, total);
            }
        }
    }

    // Взять подходящий квест у ближайшего квестодателя.
    bool TryAcceptQuest(Player* bot, std::list<Unit*> const& nearbyTargets)
    {
        if (CountActiveQuests(bot) >= MaxActiveQuests || !HasFreeQuestSlot(bot))
            return false;

        static std::set<ObjectGuid::LowType> s_scanLogged;
        uint32 givers = 0, suitable = 0, reachable = 0;
        std::string firstGiver;
        Quest const* firstSuitable = nullptr;
        int32 firstEntry = 0;

        for (Unit* unit : CollectNearbyUnits(bot, AcceptRange))
        {
            Creature* creature = unit->ToCreature();
            if (!creature || !creature->IsAlive() || !creature->IsFriendlyTo(bot))
                continue;

            auto bounds = sObjectMgr->GetCreatureQuestRelationBounds(creature->GetEntry());
            if (bounds.first != bounds.second)
            {
                ++givers;
                if (firstGiver.empty())
                    firstGiver = creature->GetName();
            }

            for (auto it = bounds.first; it != bounds.second; ++it)
            {
                Quest const* quest = sObjectMgr->GetQuestTemplate(it->second);
                if (!QuestSuitable(bot, quest))
                    continue;

                ++suitable;
                if (!firstSuitable)
                {
                    firstSuitable = quest;
                    for (uint8 j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
                        if (quest->RequiredNpcOrGo[j] > 0)
                        {
                            firstEntry = quest->RequiredNpcOrGo[j];
                            break;
                        }
                }
                // Берём квест, если цели есть в мире: они могут быть временно
                // мертвы — бот дождётся респавна у точки спавна.
                if (!HasAnyObjectiveNearby(bot, quest, nearbyTargets) && !QuestHasSpawns(bot, quest))
                    continue;

                ++reachable;

                bot->AddQuest(quest, creature);
                if (bot->GetQuestStatus(quest->GetQuestId()) != QUEST_STATUS_NONE)
                {
                    LOG_INFO("server.loading", ">> mod-ai-bots: бот {} взял квест '{}' ({}) у {}.",
                        bot->GetName(), quest->GetTitle(), quest->GetQuestId(), creature->GetName());
                    return true;
                }
            }
        }

        if (s_scanLogged.insert(bot->GetGUID().GetCounter()).second)
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} скан: гверов {}, подходящих {}, достижимых {}, юнитов {}, '{}', квест '{}' ({}), цель entry {}.",
                bot->GetName(), givers, suitable, reachable, uint32(nearbyTargets.size()), firstGiver,
                firstSuitable ? firstSuitable->GetTitle() : "", firstSuitable ? firstSuitable->GetQuestId() : 0, firstEntry);

        return false;
    }

    // Сдать выполненный квест, если рядом его эндер.
    bool TryTurnInQuest(Player* bot)
    {
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 questId = bot->GetQuestSlotQuestId(slot);
            if (!questId || bot->GetQuestStatus(questId) != QUEST_STATUS_COMPLETE)
                continue;

            Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
            if (!quest)
                continue;

            auto const key = std::make_pair(bot->GetGUID().GetCounter(), questId);
            if (s_reportedComplete.insert(key).second)
                LOG_INFO("server.loading", ">> mod-ai-bots: бот {} выполнил квест '{}' ({}), ищу эндера.",
                    bot->GetName(), quest->GetTitle(), questId);

            Creature* ender = nullptr;
            for (Unit* unit : CollectNearbyUnits(bot, HuntRange))
            {
                Creature* creature = unit->ToCreature();
                if (!creature || !creature->IsAlive() || !creature->IsFriendlyTo(bot))
                    continue;

                auto bounds = sObjectMgr->GetCreatureQuestInvolvedRelationBounds(creature->GetEntry());
                for (auto it = bounds.first; it != bounds.second; ++it)
                {
                    if (it->second == questId)
                    {
                        ender = creature;
                        break;
                    }
                }

                if (ender)
                    break;
            }

            if (!ender)
                continue;

            bot->CompleteQuest(questId);

            // Квесты с флагом TRACKING CompleteQuest награждает сразу — статус уже не COMPLETE.
            if (bot->GetQuestStatus(questId) != QUEST_STATUS_COMPLETE)
                continue;

            uint32 levelBefore = bot->GetLevel();
            uint32 xpBefore = bot->GetUInt32Value(PLAYER_XP);
            uint32 moneyBefore = bot->GetMoney();

            // Первая из наград на выбор; предметы, не влезшие в сумки, ядро отправит почтой,
            // а опыт и деньги выдаст в любом случае.
            bot->RewardQuest(quest, 0, ender, false);

            if (bot->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
            {
                if (s_reportedBlocked.insert(key).second)
                    LOG_INFO("server.loading", ">> mod-ai-bots: бот {} не смог сдать квест '{}' ({}): награда не выдана.",
                        bot->GetName(), quest->GetTitle(), questId);
                continue;
            }

            uint32 xpAfter = bot->GetUInt32Value(PLAYER_XP);
            int32 moneyGain = int32(bot->GetMoney()) - int32(moneyBefore);

            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} сдал квест '{}' ({}) у {}: опыт {}→{} (уровень {}→{}), денег {:+}.",
                bot->GetName(), quest->GetTitle(), questId, ender->GetName(),
                xpBefore, xpAfter, levelBefore, bot->GetLevel(), moneyGain);
            return true;
        }

        return false;
    }
}

namespace AiBotsQuest
{
    bool IsRequiredTarget(Player* bot, uint32 entry)
    {
        if (!bot || !entry)
            return false;
        return IsRequiredKillTarget(bot, entry);
    }

    void Update(Player* bot)
    {
        if (!bot || !bot->IsAlive() || !bot->IsInWorld())
            return;

        static std::set<ObjectGuid::LowType> s_seenBots;
        if (s_seenBots.insert(bot->GetGUID().GetCounter()).second)
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} квест-модуль активен.", bot->GetName());

        // Сдачу пробуем и в бою: иначе бот может никогда не выйти из него.
        if (TryTurnInQuest(bot))
            return;

        bool const inCombat = bot->IsInCombat();

        std::list<Unit*> hunt;
        if (!inCombat)
        {
            ReportProgress(bot);

            hunt = CollectNearbyUnits(bot, HuntRange);

            // Бросаем квест только если его целей нет в мире вообще
            // (в creature нет точек спавна), а не из-за временного отсутствия:
            // иначе бот бросал валидные квесты и простаивал.
            for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            {
                uint32 questId = bot->GetQuestSlotQuestId(slot);
                if (!questId || bot->GetQuestStatus(questId) != QUEST_STATUS_INCOMPLETE)
                    continue;

                Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
                if (!quest)
                    continue;

                if (QuestHasSpawns(bot, quest) || QuestExistsAnywhere(quest))
                    continue;

                bot->AbandonQuest(questId);
                LOG_INFO("server.loading", ">> mod-ai-bots: бот {} бросил квест '{}' ({}): целей нет в базе.",
                    bot->GetName(), quest->GetTitle(), questId);
            }
        }

        // Квест готов, но эндер далеко — идём к нему (можно и в бою).
        for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
        {
            uint32 questId = bot->GetQuestSlotQuestId(slot);
            if (!questId || bot->GetQuestStatus(questId) != QUEST_STATUS_COMPLETE)
                continue;

            if (MoveToQuestEnder(bot, questId))
                return;
        }

        // Сначала закрываем уже взятые квесты — идём к их целям,
        // и только потом ищем новых квестодателей.
        if (MoveToQuestTarget(bot))
            return;

        // Есть незакрытый квест, но цели не рядом — не идём к квестодателям:
        // иначе бот мечется между ними и целями и топчется на месте.
        if (HasIncompleteQuest(bot))
            return;

        if (!inCombat)
        {
            if (TryAcceptQuest(bot, hunt))
                return;

            MoveToQuestGiver(bot);
        }
    }
}
