/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotAI.h
 *
 * Поведение бота: следование за владельцем (для ботов игроков) или
 * автономный режим (поиск мобов/PvP, спеллы, чат).
 */

#ifndef AI_BOTS_BOT_AI_H
#define AI_BOTS_BOT_AI_H

#include "Define.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <cstdint>
#include <vector>

class Player;
class Unit;

class BotAI
{
public:
    explicit BotAI(Player* bot);

    // Вызывается периодически (раз в AiBots.UpdateInterval мс).
    void UpdateAI(uint32 diff);

    // Владелец бота (nullptr — автономный бот в мире).
    void SetOwner(Player* owner) { _owner = owner; }

    // Автономный режим: сам ищет мобов и врагов, использует спеллы, общается.
    void SetAutonomous(bool enabled) { _autonomous = enabled; if (enabled && _bot) RememberSpawn(); }
    bool IsAutonomous() const { return _autonomous; }

    // Режим осады: бот не патрулирует и держится у цели.
    void SetSiegeMode(bool enabled) { _siege = enabled; }
    bool IsSiegeMode() const { return _siege; }

    // Квестовый модуль временно перекрывает патруль (идём к цели квеста).
    void SetQuestMoveCooldown(uint32 ms) { _questMoveCooldown = ms; }

private:
    void FollowOwner();
    void UpdateCombat(uint32 diff);

    // Автономная логика
    void UpdateAutonomous(uint32 diff);
    Unit* FindTarget();
    Unit* FindPvPTarget();
    bool CastRandomSpell(Unit* target);
    void UpdateChat(uint32 diff, bool inCombat);
    void SayChat(std::vector<std::string> const& phrases, bool yell = false);

    // Тактика: лечение, баффы, отступление
    void UpdateTactics(uint32 diff);
    bool TryHeal(Unit* target);
    void TryBuff();
    bool ShouldFlee() const;
    void StartFleeing(Unit* from);
    void UpdateFleeing(uint32 diff);

    // Патрулирование вокруг точки спавна
    void UpdatePatrol(uint32 diff);
    void RememberSpawn();

    void Attack(Unit* target);
    void StopAttack();

    Player* _bot;
    Player* _owner;
    bool _autonomous;
    uint32 _attackTimer;
    uint32 _chatTimer;
    uint32 _buffTimer;
    uint32 _fleeTimer;
    uint32 _patrolTimer;
    uint32 _questTimer;
    uint32 _questMoveCooldown;
    bool   _siege = false;
    float _spawnX;
    float _spawnY;
    bool _inCombat;
    bool _fleeing;
    // Последняя цель бота: требуется после боя, когда ядро уже сбросило GetVictim().
    ObjectGuid _lastTargetGuid;
    TeamId _teamId;
    std::vector<uint32> _spells;
};

#endif // AI_BOTS_BOT_AI_H
