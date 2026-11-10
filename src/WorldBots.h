/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/WorldBots.h
 *
 * Менеджер автономных ботов: спавн в локациях по режиму из конфига,
 * респавн после смерти, обновление AI.
 */

#ifndef AI_BOTS_WORLD_BOTS_H
#define AI_BOTS_WORLD_BOTS_H

#include "Define.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <cstdint>
#include <vector>

class Player;

struct WorldBotSpawnPoint
{
    uint32 mapId;
    float x;
    float y;
    float z;
    float o;
    uint8  team = 0;      // 0=любая, 1=Альянс, 2=Орда
    uint8  category = 1;  // 0=город, 1=деревня, 2=зона фарма
};

class WorldBots
{
public:
    static WorldBots* instance();

    // Спавн всех автономных ботов при старте мира.
    void SpawnAll();

    // Периодическое обновление: респавн погибших, тик AI.
    void Update(uint32 diff);

    // Живые в мире автономные боты (для осад и социальных подсистем).
    void CollectOnlineBots(std::vector<Player*>& out) const;

    // Все guid автономных ботов (включая тех, кто ещё не в мире).
    std::vector<ObjectGuid::LowType> GetBotGuids() const;

    // Точка спавна согласно режиму из конфига (с учётом фракции бота).
    WorldBotSpawnPoint PickSpawnPoint(TeamId team, bool endgame = false) const;

private:
    WorldBots() = default;

    bool SpawnOne(uint32 index, bool endgame = false);
    void DespawnAll();

    struct WorldBot
    {
        ObjectGuid::LowType guid;
        bool alive;
        bool enterTried;
        bool admitted = false;   // допущен к входу (постепенный спавн)
        uint32 respawnTimerMs;
        uint32 xpTimerMs;
    };

    std::vector<WorldBot> _bots;
    std::vector<WorldBotSpawnPoint> _sqlPoints;
    bool _sqlPointsLoaded = false;
    uint32 _updateTimer = 0;
    uint32 _releaseTimer = 0;
};

#define sWorldBots WorldBots::instance()

#endif // AI_BOTS_WORLD_BOTS_H
