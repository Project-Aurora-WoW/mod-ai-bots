/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/Config_Bots.h
 *
 * Интерфейс конфигурации модуля.
 */

#ifndef AI_BOTS_CONFIG_BOTS_H
#define AI_BOTS_CONFIG_BOTS_H

#include "Define.h"

#include <cstdint>
#include <string>

namespace AiBotsConfig
{
    // Загрузка/перечитывание конфигурации
    void Load();

    bool    IsEnabled();            // AiBots.Enable
    uint32  GetBotAccountId();      // AiBots.AccountId
    uint32  GetMaxBotsPerPlayer();  // AiBots.MaxBotsPerPlayer
    bool    IsAutoSpawnEnabled();   // AiBots.AutoSpawn
    uint32  GetBotsPerLogin();      // AiBots.AutoSpawnCount
    uint32  GetUpdateIntervalMs();  // AiBots.UpdateInterval
    bool    IsCombatEnabled();      // AiBots.EnableCombat
    float   GetFollowDistance();    // AiBots.FollowDistance
    bool    IsCommandAvailableToAll(); // AiBots.CommandsForAll
    std::string const& GetDefaultRaceClass(); // AiBots.DefaultRaceClass, формат "race:class"
    bool    IsRandomRaceClassEnabled(); // AiBots.RandomRaceClass: случайные раса/класс в рамках фракции владельца

    // Уникальные аккаунты ботов (AiBots.UniqueAccount.*)
    bool    IsUniqueAccountEnabled();        // AiBots.UniqueAccount.Enable: каждому боту свой аккаунт
    std::string const& GetUniqueAccountPrefix(); // AiBots.UniqueAccount.Prefix: префикс имён аккаунтов
    uint32  GetUniqueAccountStartId();       // AiBots.UniqueAccount.StartId: стартовый номер аккаунта

    // Автономные боты в мире (AiBots.WorldBots.*)
    bool    IsWorldBotsEnabled();       // AiBots.WorldBots.Enable
    uint32  GetWorldBotsCount();        // AiBots.WorldBots.Count
    uint8   GetWorldBotsSpawnMode();    // AiBots.WorldBots.SpawnMode: 0=SQL, 1=у игроков, 2=стартовые зоны
    uint32  GetWorldBotsRespawnSeconds(); // AiBots.WorldBots.RespawnSeconds
    bool    IsWorldBotsSpellsEnabled(); // AiBots.WorldBots.UseSpells
    bool    IsWorldBotsChatEnabled();   // AiBots.WorldBots.Chat.Enable
    uint32  GetBattleCryChance();       // AiBots.WorldBots.Chat.BattleCryChance
    uint32  GetIdleChatChance();        // AiBots.WorldBots.Chat.IdleChance

    // Прокачка и тактика (AiBots.Tactics.*)
    uint32  GetWorldBotLevel();         // AiBots.WorldBots.Level
    uint32  GetWorldBotLevelMin();      // AiBots.WorldBots.LevelMin
    uint32  GetWorldBotLevelMax();      // AiBots.WorldBots.LevelMax
    uint32  GetSyncOwnerLevel();        // AiBots.SyncOwnerLevel
    uint32  GetFleeHealthPct();         // AiBots.Tactics.FleeHealthPct
    uint32  GetHealThresholdPct();      // AiBots.Tactics.HealThresholdPct
    bool    IsBuffsEnabled();           // AiBots.Tactics.UseBuffs

    // Генерация имён
    uint8   GetNamesMode();             // AiBots.Names.Mode: 0=таблица, 1=генератор, 2=смешанный

    // Живой мир (AiBots.WorldBots.*)
    float   GetPatrolRadius();          // AiBots.WorldBots.Patrol.Radius
    bool    IsLevelingEnabled();        // AiBots.WorldBots.Leveling
    bool    IsWorldBotsPvPEnabled();    // AiBots.WorldBots.PvP.Enable
    uint32  GetWorldBotsPvPShare();     // AiBots.WorldBots.PvP.SharePercent
    bool    IsEndgameBotsEnabled();     // AiBots.WorldBots.Endgame.Enable
    uint32  GetEndgameBotsCount();      // AiBots.WorldBots.Endgame.Count
    uint32  GetEndgameLevelMin();       // AiBots.WorldBots.Endgame.LevelMin
    uint32  GetEndgameLevelMax();       // AiBots.WorldBots.Endgame.LevelMax
    uint32  GetWorldBotsSpawnIntervalSec();  // AiBots.WorldBots.Spawn.IntervalSeconds
    uint32  GetWorldBotsSpawnStartupCount(); // AiBots.WorldBots.Spawn.StartupCount
    uint32  GetWorldBotsProfilesNewPercent(); // AiBots.WorldBots.Profiles.NewPercent
    bool    IsProgressionEnabled();     // AiBots.WorldBots.Progression.Enable
    uint32  GetProgressionMaxLevel();   // AiBots.WorldBots.Progression.MaxLevel
    float   GetProgressionXpRate();     // AiBots.WorldBots.Progression.XpRate
    bool    IsProgressionAutoLevelEnabled(); // AiBots.WorldBots.Progression.AutoLevel
    uint32  GetProgressionAutoLevelIntervalSec(); // ...AutoLevel.IntervalSeconds
    uint32  GetEliteChance();           // AiBots.WorldBots.EliteChance
    bool    IsDuelsEnabled();           // AiBots.WorldBots.Duels.Enable
    uint32  GetDuelChance();            // AiBots.WorldBots.Duels.Chance
    bool    IsGroupsEnabled();          // AiBots.WorldBots.Groups.Enable
    uint32  GetGroupMaxSize();          // AiBots.WorldBots.Groups.MaxSize
    bool    IsVendorsEnabled();         // AiBots.Vendors.Enable
    uint32  GetVendorsCount();          // AiBots.Vendors.Count
    bool    IsTerritoryEnabled();       // AiBots.Territory.Enable
    uint32  GetTerritoryFlipMinutes();  // AiBots.Territory.FlipMinutes

    // Гильдия ботов (AiBots.Guild.*) — отдельная для каждой фракции
    bool    IsGuildEnabled();           // AiBots.Guild.Enable
    std::string const& GetAllianceGuildName(); // AiBots.Guild.AllianceName
    std::string const& GetHordeGuildName();    // AiBots.Guild.HordeName
    uint32  GetGuildXpPerKill();        // AiBots.Guild.XpPerKill
    uint32  GetGuildXpPerLevelBase();   // AiBots.Guild.XpPerLevelBase
    uint32  GetGuildMaxLevel();         // AiBots.Guild.MaxLevel

    // Осады (AiBots.Siege.*)
    bool    IsSiegeEnabled();           // AiBots.Siege.Enable
    uint32  GetSiegeIntervalMin();      // AiBots.Siege.IntervalMinMinutes
    uint32  GetSiegeIntervalMax();      // AiBots.Siege.IntervalMaxMinutes
    uint32  GetSiegeAttackerMin();      // AiBots.Siege.AttackerMin
    uint32  GetSiegeAttackerMax();      // AiBots.Siege.AttackerMax
    uint32  GetSiegeDurationMinutes();  // AiBots.Siege.DurationMinutes

    // Питомцы (AiBots.Pets.*)
    bool    IsPetsEnabled();            // AiBots.Pets.Enable
    uint32  GetPetsSummonInterval();    // AiBots.Pets.SummonIntervalSeconds
}

#endif // AI_BOTS_CONFIG_BOTS_H
