/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotAI.cpp
 *
 * Реализация поведения: следование за владельцем, melee-атака, спеллы
 * по классу, PvP бот-против-бота и чат для автономных ботов.
 */

#include "BotAI.h"
#include "BotMgr.h"
#include "BotSpells.h"
#include "BotPersonality.h"
#include "BotLoot.h"
#include "BotSocial.h"
#include "BotGroups.h"
#include "BotGuild.h"
#include "BotQuest.h"
#include "BotSkills.h"
#include "BotInteract.h"
#include "BotStats.h"
#include "BotBattleground.h"
#include "BotDialogue.h"
#include "BotChat.h"
#include "BotGather.h"
#include "Config_Bots.h"

#include "Cell.h"
#include "CellImpl.h"
#include "Chat.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellMgr.h"
#include "Unit.h"

#include <cmath>

namespace
{
    float const MaxAttackRange = 5.0f;      // дистанция melee-атаки
    float const MaxSpellRange = 30.0f;      // дистанция каста
    float const SearchTargetRange = 30.0f;  // радиус поиска мобов
    float const SearchPvPRange = 40.0f;     // радиус поиска PvP-целей
    uint32 const AttackCooldownMs = 2000;
    uint32 const ChatCheckIntervalMs = 10000;
    uint32 const PatrolIntervalMs = 8000; // новая точка патруля раз в 8 сек
}

BotAI::BotAI(Player* bot)
    : _bot(bot), _owner(nullptr), _autonomous(false), _attackTimer(0), _chatTimer(0), _buffTimer(0), _fleeTimer(0)
    , _patrolTimer(0), _questTimer(0), _questMoveCooldown(0), _spawnX(0.0f), _spawnY(0.0f), _inCombat(false), _fleeing(false), _lastTargetGuid(), _teamId(TEAM_NEUTRAL)
{
    if (bot)
        _spells = AiBotsSpells::GetSpellsForClass(bot->getClass());
}

void BotAI::UpdateAI(uint32 diff)
{
    if (!_bot || !_bot->IsInWorld() || _bot->IsBeingTeleported())
        return;

    if (_questMoveCooldown > diff)
        _questMoveCooldown -= diff;
    else
        _questMoveCooldown = 0;

    AiBotsSkills::UpdateProfessions(_bot, diff);
    AiBotsInteract::Update(_bot);
    AiBotsStats::Update(_bot, diff);
    AiBotsBattleground::Update(_bot, diff);
    AiBotsDialogue::Update(_bot, diff);
    AiBotsChat::Update(_bot, diff);

    if (_autonomous)
    {
        // Автономные боты тоже лечатся, баффуются и отступают при низком HP.
        UpdateTactics(diff);

        if (_fleeing && !_siege)
        {
            UpdateFleeing(diff);
            return;
        }

        UpdateAutonomous(diff);
        return;
    }

    // Тактика общая для обоих режимов: лечение/баффы/отступление.
    UpdateTactics(diff);

    if (_fleeing)
    {
        UpdateFleeing(diff);
        return;
    }

    FollowOwner();
    UpdateCombat(diff);
}

void BotAI::FollowOwner()
{
    if (!_owner || !_owner->IsInWorld())
        return;

    // В бою не перебиваем погоню: MoveFollow каждые полсекунды срывал
    // атаку, и спутник только ходил за владельцем.
    if (_bot->IsInCombat() || _bot->GetVictim())
        return;

    float dist = _bot->GetDistance(_owner);
    if (dist > AiBotsConfig::GetFollowDistance() + 2.0f)
        _bot->GetMotionMaster()->MoveFollow(_owner, AiBotsConfig::GetFollowDistance(), 0.0f);
}

void BotAI::UpdateCombat(uint32 diff)
{
    if (!AiBotsConfig::IsCombatEnabled())
        return;

    _attackTimer += diff;
    if (_attackTimer < AttackCooldownMs)
        return;
    _attackTimer = 0;

    // Дуэль: бьём противника.
    if (_bot->duel && _bot->duel->State == DUEL_STATE_IN_PROGRESS)
    {
        Player* opponent = _bot->duel->Opponent;
        if (opponent && opponent->IsAlive() && _bot->GetDistance(opponent) <= MaxSpellRange)
        {
            Attack(opponent);
            return;
        }
    }

    // Помощь владельцу: атакуем того, с кем сражается он.
    if (_owner && _owner->IsInWorld())
    {
        if (Unit* victim = _owner->GetVictim())
        {
            if (victim->IsAlive() && _bot->IsValidAttackTarget(victim) &&
                _bot->GetDistance(victim) <= MaxAttackRange)
            {
                Attack(victim);
                return;
            }
        }
    }

    // Цель, выбранная самим ботом
    if (Unit* target = _bot->GetSelectedUnit())
    {
        if (target->IsAlive() && target->IsHostileTo(_bot) &&
            _bot->GetDistance(target) <= MaxAttackRange)
        {
            _bot->AttackerStateUpdate(target);
            return;
        }
    }

    if (Unit* target = FindTarget())
        Attack(target);
}

// ---------- Автономный режим ----------

void BotAI::UpdateAutonomous(uint32 diff)
{
    if (!_bot->IsAlive())
    {
        _inCombat = false;
        return;
    }

    // Квесты: тик раз в 3 секунды; сам модуль пропускает бой/мёртвых.
    _questTimer += diff;
    if (_questTimer >= 3000)
    {
        _questTimer = 0;
        AiBotsQuest::Update(_bot);
    }

    // Квест-марш: идём к цели квеста и не отвлекаемся, пока никто не бьёт.
    if (_questMoveCooldown > 0 && !_bot->GetVictim() && _bot->isMoving())
    {
        _inCombat = false;
        _fleeing = false;
        return;
    }

    // Сбор ресурсов (трава/руда) вне боя, отступления и осады.
    if (!_siege && !_fleeing)
        AiBotsGather::Update(_bot, diff);

    // Патрулирование, когда нет боя и бот не в осаде.
    if (!_inCombat && !_fleeing && !_siege && _questMoveCooldown == 0 && AiBotsConfig::GetPatrolRadius() > 0.0f)
        UpdatePatrol(diff);

    bool combatStarted = false;
    // Приоритет PvP: боты разных фракций атакуют друг друга, даже если рядом есть мобы.
    Unit* target = FindPvPTarget();
    if (!target)
        target = FindTarget();

    if (target)
    {
        if (!_inCombat && _siege)
            LOG_INFO("server.loading", ">> mod-ai-bots: {} в осаде атакует {}.", _bot->GetName(), target->GetName());

        if (!_inCombat && target->IsPlayer())
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} атакует противника {} (PvP).", _bot->GetName(), target->GetName());

        if (!_inCombat && AiBotsConfig::IsWorldBotsChatEnabled())
        {
            SayChat(AiBotsPersonality::GetForGuid(_bot->GetGUID().GetCounter()).battleCries,
                    roll_chance_i(AiBotsConfig::GetBattleCryChance() / 2));
            combatStarted = true;
        }

        _inCombat = true;
        _lastTargetGuid = target->GetGUID();
        Attack(target);
    }
    else
    {
        if (_inCombat)
        {
            // Бой окончен — фраза победы, сбор добычи, XP гильдии.
            // Цель берём по запомненному guid: GetVictim() уже сброшен ядром.
            _inCombat = false;
            if (Unit* victim = ObjectAccessor::GetUnit(*_bot, _lastTargetGuid))
            {
                if (!victim->IsAlive())
                {
                    AiBotsLoot::CollectFrom(_bot, victim);
                    AiBotsStats::OnCreatureKill(_bot, victim);
                    AiBotsGuild::AwardKillXp(_bot, victim);
                }
            }
            _lastTargetGuid.Clear();

            if (AiBotsConfig::IsWorldBotsChatEnabled() && roll_chance_i(AiBotsConfig::GetBattleCryChance()))
                SayChat(AiBotsPersonality::GetForGuid(_bot->GetGUID().GetCounter()).victory);
        }
        StopAttack();
    }

    UpdateChat(diff, _inCombat || combatStarted);
}

// ---------- Патрулирование ----------

void BotAI::UpdatePatrol(uint32 diff)
{
    _patrolTimer += diff;
    if (_patrolTimer < PatrolIntervalMs)
        return;
    _patrolTimer = 0;

    float radius = AiBotsConfig::GetPatrolRadius();
    float angle = frand(0.0f, 2.0f * float(M_PI));
    float dist = frand(radius * 0.3f, radius);
    float x = _spawnX + dist * std::cos(angle);
    float y = _spawnY + dist * std::sin(angle);
    float z = _bot->GetPositionZ();

    _bot->GetMotionMaster()->MovePoint(1, x, y, z);
}

void BotAI::RememberSpawn()
{
    _spawnX = _bot->GetPositionX();
    _spawnY = _bot->GetPositionY();
}

Unit* BotAI::FindTarget()
{
    // Ищем ближайшего живого моба, враждебного боту.
    float range = _siege ? 80.0f : SearchTargetRange;
    std::list<Unit*> targets;
    Acore::AnyUnitInObjectRangeCheck check(_bot, range);
    Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(_bot, targets, check);
    TypeContainerVisitor<Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck>, GridTypeMapContainer> visitor(searcher);
    Cell cell(Acore::ComputeCellCoord(_bot->GetPositionX(), _bot->GetPositionY()));
    cell.Visit(Acore::ComputeCellCoord(_bot->GetPositionX(), _bot->GetPositionY()), visitor, *_bot->GetMap(), *_bot, range);

    Unit* best = nullptr;
    float bestDist = range;
    for (Unit* unit : targets)
    {
        if (!unit->IsAlive() || unit->IsPlayer())
            continue;

        // Цель квеста может быть нейтральной — её тоже бьём.
        bool questTarget = unit->ToCreature() && AiBotsQuest::IsRequiredTarget(_bot, unit->GetEntry());
        if (!questTarget && !unit->IsHostileTo(_bot))
            continue;

        // Не трогаем существ с пустым индикатором агрессии (декоративные NPC).
        if (!questTarget && unit->GetReactionTo(_bot) == REP_FRIENDLY)
            continue;

        float dist = _bot->GetDistance(unit);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = unit;
        }
    }

    return best;
}

Unit* BotAI::FindPvPTarget()
{
    // PvP разрешён на полях сражений/аренах и в Wintergrasp.
    if (!_bot->InBattleground() && _bot->GetZoneId() != 4197)
        return nullptr;

    // PvP: враждебный игрок рядом (реальный или бот), только автономные боты.
    std::list<Player*> targets;
    Acore::AnyPlayerInObjectRangeCheck check(_bot, SearchPvPRange);
    Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(_bot, targets, check);
    TypeContainerVisitor<Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck>, GridTypeMapContainer> visitor(searcher);
    Cell cell(Acore::ComputeCellCoord(_bot->GetPositionX(), _bot->GetPositionY()));
    cell.Visit(Acore::ComputeCellCoord(_bot->GetPositionX(), _bot->GetPositionY()), visitor, *_bot->GetMap(), *_bot, SearchPvPRange);

    Unit* best = nullptr;
    float bestDist = SearchPvPRange;
    for (Player* player : targets)
    {
        if (!player->IsAlive() || player == _bot || player->IsGameMaster())
            continue;

        // Враждебный игрок (реальный или бот), которого можно атаковать.
        if (!_bot->IsValidAttackTarget(player))
            continue;

        if (player->GetGroup() && player->GetGroup() == _bot->GetGroup())
            continue;

        float dist = _bot->GetDistance(player);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = player;
        }
    }

    return best;
}

bool BotAI::CastRandomSpell(Unit* target)
{
    if (_spells.empty() || !target)
        return false;

    // Пробуем случайные спеллы, пока один не скастуется.
    uint32 attempts = _spells.size();
    while (attempts-- > 0)
    {
        uint32 spellId = _spells[urand(0, _spells.size() - 1)];
        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        if (!_bot->HasSpell(spellId))
            continue;

        float range = spellInfo->GetMaxRange();
        if (_bot->GetDistance(target) > range)
            continue;

        if (!_bot->HasSpellCooldown(spellId))
        {
            _bot->CastSpell(target, spellId, true);
            return true;
        }
    }

    return false;
}

void BotAI::UpdateChat(uint32 diff, bool inCombat)
{
    if (!AiBotsConfig::IsWorldBotsChatEnabled() || inCombat)
        return;

    _chatTimer += diff;
    if (_chatTimer < ChatCheckIntervalMs)
        return;
    _chatTimer = 0;

    if (roll_chance_i(AiBotsConfig::GetIdleChatChance()))
        SayChat(AiBotsPersonality::GetForGuid(_bot->GetGUID().GetCounter()).idle);
}

void BotAI::SayChat(std::vector<std::string> const& phrases, bool yell)
{
    if (phrases.empty())
        return;

    std::string const& phrase = phrases[urand(0, phrases.size() - 1)];

    LOG_DEBUG("server.loading", ">> mod-ai-bots: {} говорит: {}", _bot->GetName(), phrase);

    // Say или Yell — но не оба сразу, чтобы игроки не видели реплику дважды.
    if (yell)
        _bot->Yell(phrase, LANG_UNIVERSAL);
    else
        _bot->Say(phrase, LANG_UNIVERSAL);
}

// ---------- Тактика: лечение, баффы, отступление ----------

void BotAI::UpdateTactics(uint32 diff)
{
    if (!_bot->IsAlive())
        return;

    // Отступление при низком HP (в осаде не отступаем).
    if (!_siege && AiBotsConfig::GetFleeHealthPct() > 0 && ShouldFlee())
    {
        if (!_fleeing)
            StartFleeing(_bot->GetSelectedUnit() ? _bot->GetSelectedUnit() : _bot->GetVictim());
        return;
    }

    // Лечение при низком HP (себя, затем владельца).
    if (AiBotsConfig::GetHealThresholdPct() > 0)
    {
        if (_bot->GetHealthPct() < AiBotsConfig::GetHealThresholdPct())
        {
            if (TryHeal(_bot))
                return;
        }
        else
        {
            Unit* healTarget = nullptr;

            if (_owner && _owner->IsInWorld() && _owner->IsAlive() &&
                _owner->GetHealthPct() < AiBotsConfig::GetHealThresholdPct())
            {
                healTarget = _owner;
            }
            else if (_owner && _owner->GetGroup())
            {
                for (auto const& slot : _owner->GetGroup()->GetMemberSlots())
                {
                    Player* member = ObjectAccessor::FindConnectedPlayer(slot.guid);
                    if (!member || !member->IsInWorld() || !member->IsAlive())
                        continue;
                    if (member->GetHealthPct() >= AiBotsConfig::GetHealThresholdPct())
                        continue;
                    if (!_bot->IsFriendlyTo(member) || _bot->GetDistance(member) > 30.0f)
                        continue;

                    healTarget = member;
                    break;
                }
            }

            if (healTarget)
                TryHeal(healTarget);
        }
    }

    // Баффы — раз в 30 секунд проверяем.
    _buffTimer += diff;
    if (AiBotsConfig::IsBuffsEnabled() && _buffTimer >= 30000)
    {
        _buffTimer = 0;
        TryBuff();
    }
}

bool BotAI::TryHeal(Unit* target)
{
    if (!target || !target->IsAlive())
        return false;

    auto const& heals = AiBotsSpells::GetHealSpellsForClass(_bot->getClass());
    if (heals.empty())
        return false;

    for (uint32 spellId : heals)
    {
        if (!_bot->HasSpell(spellId))
            continue;

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo)
            continue;

        if (_bot->HasSpellCooldown(spellId))
            continue;

        if (_bot->GetDistance(target) > spellInfo->GetMaxRange())
            continue;

        _bot->CastSpell(target, spellId, true);

        // Изредка бот сообщает о лечении в чат.
        if (_autonomous && AiBotsConfig::IsWorldBotsChatEnabled() && roll_chance_i(20))
            SayChat(AiBotsPersonality::GetForGuid(_bot->GetGUID().GetCounter()).heal);

        return true;
    }

    return false;
}

void BotAI::TryBuff()
{
    auto const& buffs = AiBotsSpells::GetBuffSpellsForClass(_bot->getClass());
    if (buffs.empty())
        return;

    for (uint32 spellId : buffs)
    {
        if (!_bot->HasSpell(spellId))
            continue;

        if (_bot->HasSpellCooldown(spellId))
            continue;

        // Кастуем на себя, если бафф ещё не висит.
        if (!_bot->HasAura(spellId))
            _bot->CastSpell(_bot, spellId, true);

        // Бафф владельца, если он в той же фракции и рядом.
        if (_owner && _owner->IsInWorld() && _owner->IsFriendlyTo(_bot) &&
            _bot->GetDistance(_owner) <= 30.0f && !_owner->HasAura(spellId))
        {
            _bot->CastSpell(_owner, spellId, true);
        }
    }
}

bool BotAI::ShouldFlee() const
{
    if (!_bot->IsInCombat())
        return false;

    return _bot->GetHealthPct() < AiBotsConfig::GetFleeHealthPct();
}

void BotAI::StartFleeing(Unit* from)
{
    _fleeing = true;
    _fleeTimer = 8000; // до 8 секунд отступления

    StopAttack();

    if (from && _autonomous && AiBotsConfig::IsWorldBotsChatEnabled() && roll_chance_i(50))
        SayChat(AiBotsPersonality::GetForGuid(_bot->GetGUID().GetCounter()).flee);

    // Убегаем в противоположном направлении от угрозы.
    float fleeAngle = from ? _bot->GetAngle(from) + float(M_PI) : frand(0.0f, 2.0f * float(M_PI));
    float fleeDist = 30.0f;
    float x = _bot->GetPositionX() + fleeDist * std::cos(fleeAngle);
    float y = _bot->GetPositionY() + fleeDist * std::sin(fleeAngle);
    float z = _bot->GetPositionZ();

    _bot->GetMotionMaster()->MovePoint(0, x, y, z);
}

void BotAI::UpdateFleeing(uint32 diff)
{
    // Выходим из режима отступления по таймеру или когда бой окончен.
    if (_fleeTimer <= diff || !_bot->IsInCombat())
    {
        _fleeing = false;
        _fleeTimer = 0;
        if (!_autonomous && _owner && _owner->IsInWorld())
            _bot->GetMotionMaster()->MoveFollow(_owner, AiBotsConfig::GetFollowDistance(), 0.0f);
    }
    else
        _fleeTimer -= diff;
}

// ---------- Общее ----------

void BotAI::Attack(Unit* target)
{
    if (!target || !_bot->IsAlive())
        return;

    _bot->SetSelection(target->GetGUID());

    float dist = _bot->GetDistance(target);
    bool hasRangedSpells = AiBotsConfig::IsWorldBotsSpellsEnabled() && !_spells.empty();

    // Кастерам с рэнж-спеллами выгоднее держать дистанцию каста, melee — 5 ярдов.
    float chaseRange = hasRangedSpells ? MaxSpellRange - 2.0f : MaxAttackRange - 1.0f;
    if (dist > chaseRange)
        _bot->GetMotionMaster()->MoveChase(target, chaseRange);

    // Спеллы по классу, если включены.
    if (hasRangedSpells && dist <= MaxSpellRange)
    {
        if (CastRandomSpell(target))
            return;
    }

    // Иначе обычная автоатака.
    if (dist <= MaxAttackRange)
        _bot->AttackerStateUpdate(target);
}

void BotAI::StopAttack()
{
    _bot->AttackStop();
    _bot->GetMotionMaster()->Clear();
    _lastTargetGuid.Clear();
}
