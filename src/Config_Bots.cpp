/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/Config_Bots.cpp
 */

#include "Config_Bots.h"

#include "Config.h"

namespace
{
    bool        s_enabled       = false;
    uint32      s_botAccountId  = 0;
    uint32      s_maxBots       = 1;
    bool        s_uniqueAccount   = false;
    std::string s_uniquePrefix    = "AIBOT";
    uint32      s_uniqueStartId   = 10000;
    bool        s_autoSpawn     = false;
    uint32      s_botsPerLogin  = 1;
    uint32      s_updateMs      = 500;
    bool        s_combat        = true;
    float       s_followDist    = 4.0f;
    bool        s_cmdAll        = true;
    std::string s_defaultRC     = "1:1"; // human warrior
    bool        s_randomRC      = true;  // случайные раса/класс в рамках фракции владельца

    // Автономные боты
    bool        s_worldEnabled      = false;
    uint32      s_worldCount        = 10;
    uint8       s_worldSpawnMode    = 0;
    uint32      s_worldRespawnSec   = 60;
    bool        s_worldSpells       = true;
    bool        s_worldChat         = true;
    uint32      s_battleCryChance   = 30;
    uint32      s_idleChatChance    = 2;

    // Прокачка и тактика
    uint32      s_worldBotLevel     = 5;
    uint32      s_worldBotLevelMin  = 1;
    uint32      s_worldBotLevelMax  = 5;
    uint32      s_syncOwnerLevel    = 0;
    uint32      s_fleeHealthPct     = 20;
    uint32      s_healThresholdPct  = 50;
    bool        s_useBuffs          = true;

    // Генерация имён
    uint8       s_namesMode         = 2;

    // Живой мир
    float       s_patrolRadius      = 25.0f;
    bool        s_leveling          = true;

    // Прогрессия ботов
    bool        s_progressionEnabled   = true;
    uint32      s_progressionMaxLevel  = 60;
    float       s_progressionXpRate    = 1.0f;
    bool        s_progressionAutoLevel = false;
    uint32      s_progressionAutoSec   = 300;

    // PvP между ботами разных фракций
    bool        s_worldPvP          = true;
    uint32      s_worldPvPShare     = 50;

    // Эндгейм-боты 70-80 уровня (арены, Wintergrasp, Нордскол)
    bool        s_endgameEnabled    = true;
    uint32      s_endgameCount      = 5;
    uint32      s_endgameLevelMin   = 70;
    uint32      s_endgameLevelMax   = 80;

    // Постепенный спавн: сразу StartupCount, далее по одному за интервал.
    uint32      s_spawnIntervalSec  = 20;
    uint32      s_spawnStartupCount = 5;

    // Доля слотов, которые занимают новые персонажи с другими профилями.
    uint32      s_profilesNewPercent = 20;

    uint32      s_eliteChance       = 10;
    bool        s_duels             = true;
    uint32      s_duelChance        = 5;
    bool        s_groups            = true;
    uint32      s_groupMaxSize      = 5;
    bool        s_vendors           = true;
    uint32      s_vendorsCount      = 3;
    bool        s_territory         = true;
    uint32      s_territoryFlipMin  = 30;

    // Гильдия
    bool        s_guildEnabled      = true;
    std::string s_guildAllianceName = "AI Vanguard";
    std::string s_guildHordeName    = "AI Legion";
    uint32      s_guildXpPerKill    = 5;
    uint32      s_guildXpBase       = 1000;
    uint32      s_guildMaxLevel     = 25;

    // Осады
    bool        s_siegeEnabled      = true;
    uint32      s_siegeIntervalMin  = 90;
    uint32      s_siegeIntervalMax  = 240;
    uint32      s_siegeAttackerMin  = 5;
    uint32      s_siegeAttackerMax  = 10;
    uint32      s_siegeDurationMin  = 15;

    // Питомцы
    bool        s_petsEnabled       = true;
    uint32      s_petsSummonSec     = 20;
}

namespace AiBotsConfig
{
    void Load()
    {
        s_enabled      = sConfigMgr->GetOption<bool>("AiBots.Enable", false);
        s_botAccountId = sConfigMgr->GetOption<uint32>("AiBots.AccountId", 0);
        s_maxBots      = sConfigMgr->GetOption<uint32>("AiBots.MaxBotsPerPlayer", 1);
        s_uniqueAccount = sConfigMgr->GetOption<bool>("AiBots.UniqueAccount.Enable", false);
        s_uniquePrefix  = sConfigMgr->GetOption<std::string>("AiBots.UniqueAccount.Prefix", "AIBOT");
        s_uniqueStartId = sConfigMgr->GetOption<uint32>("AiBots.UniqueAccount.StartId", 10000);
        s_autoSpawn    = sConfigMgr->GetOption<bool>("AiBots.AutoSpawn", false);
        s_botsPerLogin = sConfigMgr->GetOption<uint32>("AiBots.AutoSpawnCount", 1);
        s_updateMs     = sConfigMgr->GetOption<uint32>("AiBots.UpdateInterval", 500);
        s_combat       = sConfigMgr->GetOption<bool>("AiBots.EnableCombat", true);
        s_followDist   = sConfigMgr->GetOption<float>("AiBots.FollowDistance", 4.0f);
        s_cmdAll       = sConfigMgr->GetOption<bool>("AiBots.CommandsForAll", true);
        s_defaultRC    = sConfigMgr->GetOption<std::string>("AiBots.DefaultRaceClass", "1:1");
        s_randomRC     = sConfigMgr->GetOption<bool>("AiBots.RandomRaceClass", true);

        s_worldEnabled     = sConfigMgr->GetOption<bool>("AiBots.WorldBots.Enable", false);
        s_worldCount       = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Count", 10);
        if (s_worldCount > 200) // защита от случайного огромного значения
            s_worldCount = 200;
        s_worldSpawnMode   = sConfigMgr->GetOption<uint8>("AiBots.WorldBots.SpawnMode", 0);
        s_worldRespawnSec  = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.RespawnSeconds", 60);
        s_worldSpells      = sConfigMgr->GetOption<bool>("AiBots.WorldBots.UseSpells", true);
        s_worldChat        = sConfigMgr->GetOption<bool>("AiBots.WorldBots.Chat.Enable", true);
        s_battleCryChance  = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Chat.BattleCryChance", 30);
        s_idleChatChance   = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Chat.IdleChance", 2);

        s_worldBotLevel    = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Level", 5);
        s_worldBotLevelMin = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.LevelMin", s_worldBotLevel);
        s_worldBotLevelMax = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.LevelMax", s_worldBotLevel);
        if (s_worldBotLevelMin < 1)
            s_worldBotLevelMin = 1;
        if (s_worldBotLevelMax > 80)
            s_worldBotLevelMax = 80;
        if (s_worldBotLevelMin > s_worldBotLevelMax)
        {
            uint32 tmp = s_worldBotLevelMin;
            s_worldBotLevelMin = s_worldBotLevelMax;
            s_worldBotLevelMax = tmp;
        }
        s_syncOwnerLevel   = sConfigMgr->GetOption<uint32>("AiBots.SyncOwnerLevel", 0);
        s_fleeHealthPct    = sConfigMgr->GetOption<uint32>("AiBots.Tactics.FleeHealthPct", 20);
        s_healThresholdPct = sConfigMgr->GetOption<uint32>("AiBots.Tactics.HealThresholdPct", 50);
        s_useBuffs         = sConfigMgr->GetOption<bool>("AiBots.Tactics.UseBuffs", true);

        s_namesMode        = sConfigMgr->GetOption<uint8>("AiBots.Names.Mode", 2);

        s_patrolRadius     = sConfigMgr->GetOption<float>("AiBots.WorldBots.Patrol.Radius", 25.0f);
        s_leveling         = sConfigMgr->GetOption<bool>("AiBots.WorldBots.Leveling", true);
        s_progressionEnabled   = sConfigMgr->GetOption<bool>("AiBots.WorldBots.Progression.Enable", true);
        s_progressionMaxLevel  = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Progression.MaxLevel", s_worldBotLevelMax);
        if (s_progressionMaxLevel < 1)
            s_progressionMaxLevel = 1;
        if (s_progressionMaxLevel > 80)
            s_progressionMaxLevel = 80;
        s_progressionXpRate    = sConfigMgr->GetOption<float>("AiBots.WorldBots.Progression.XpRate", 1.0f);
        if (s_progressionXpRate < 0.0f)
            s_progressionXpRate = 0.0f;
        if (s_progressionXpRate > 20.0f)
            s_progressionXpRate = 20.0f;
        s_progressionAutoLevel = sConfigMgr->GetOption<bool>("AiBots.WorldBots.Progression.AutoLevel", s_leveling);
        s_progressionAutoSec   = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Progression.AutoLevel.IntervalSeconds", 300);
        if (s_progressionAutoSec < 10)
            s_progressionAutoSec = 10;
        s_worldPvP      = sConfigMgr->GetOption<bool>("AiBots.WorldBots.PvP.Enable", true);
        s_worldPvPShare = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.PvP.SharePercent", 50);
        if (s_worldPvPShare > 100)
            s_worldPvPShare = 100;
        s_endgameEnabled  = sConfigMgr->GetOption<bool>("AiBots.WorldBots.Endgame.Enable", true);
        s_endgameCount    = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Endgame.Count", 5);
        s_endgameLevelMin = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Endgame.LevelMin", 70);
        s_endgameLevelMax = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Endgame.LevelMax", 80);
        if (s_endgameLevelMin < 70)
            s_endgameLevelMin = 70;
        if (s_endgameLevelMax > 80)
            s_endgameLevelMax = 80;
        if (s_endgameLevelMin > s_endgameLevelMax)
        {
            uint32 tmp = s_endgameLevelMin;
            s_endgameLevelMin = s_endgameLevelMax;
            s_endgameLevelMax = tmp;
        }
        if (s_endgameCount > 50)
            s_endgameCount = 50;
        s_spawnIntervalSec  = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Spawn.IntervalSeconds", 20);
        s_spawnStartupCount = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Spawn.StartupCount", 5);
        s_profilesNewPercent = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Profiles.NewPercent", 20);
        if (s_profilesNewPercent > 100)
            s_profilesNewPercent = 100;
        s_eliteChance      = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.EliteChance", 10);
        s_duels            = sConfigMgr->GetOption<bool>("AiBots.WorldBots.Duels.Enable", true);
        s_duelChance       = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Duels.Chance", 5);
        s_groups           = sConfigMgr->GetOption<bool>("AiBots.WorldBots.Groups.Enable", true);
        s_groupMaxSize     = sConfigMgr->GetOption<uint32>("AiBots.WorldBots.Groups.MaxSize", 5);
        s_vendors          = sConfigMgr->GetOption<bool>("AiBots.Vendors.Enable", true);
        s_vendorsCount     = sConfigMgr->GetOption<uint32>("AiBots.Vendors.Count", 3);
        s_territory        = sConfigMgr->GetOption<bool>("AiBots.Territory.Enable", true);
        s_territoryFlipMin = sConfigMgr->GetOption<uint32>("AiBots.Territory.FlipMinutes", 30);

        s_guildEnabled     = sConfigMgr->GetOption<bool>("AiBots.Guild.Enable", true);
        s_guildAllianceName = sConfigMgr->GetOption<std::string>("AiBots.Guild.AllianceName", "AI Vanguard");
        s_guildHordeName    = sConfigMgr->GetOption<std::string>("AiBots.Guild.HordeName", "AI Legion");
        s_guildXpPerKill   = sConfigMgr->GetOption<uint32>("AiBots.Guild.XpPerKill", 5);
        s_guildXpBase      = sConfigMgr->GetOption<uint32>("AiBots.Guild.XpPerLevelBase", 1000);
        s_guildMaxLevel    = sConfigMgr->GetOption<uint32>("AiBots.Guild.MaxLevel", 25);

        s_siegeEnabled     = sConfigMgr->GetOption<bool>("AiBots.Siege.Enable", true);
        s_siegeIntervalMin = sConfigMgr->GetOption<uint32>("AiBots.Siege.IntervalMinMinutes", 90);
        s_siegeIntervalMax = sConfigMgr->GetOption<uint32>("AiBots.Siege.IntervalMaxMinutes", 240);
        s_siegeAttackerMin = sConfigMgr->GetOption<uint32>("AiBots.Siege.AttackerMin", 5);
        s_siegeAttackerMax = sConfigMgr->GetOption<uint32>("AiBots.Siege.AttackerMax", 10);
        s_siegeDurationMin = sConfigMgr->GetOption<uint32>("AiBots.Siege.DurationMinutes", 15);

        s_petsEnabled      = sConfigMgr->GetOption<bool>("AiBots.Pets.Enable", true);
        s_petsSummonSec    = sConfigMgr->GetOption<uint32>("AiBots.Pets.SummonIntervalSeconds", 20);

        // Защита от некорректных значений: min > max роняет urand.
        if (s_followDist < 1.0f)
            s_followDist = 1.0f;
        if (s_groupMaxSize < 2)
            s_groupMaxSize = 2;
        if (s_vendorsCount < 1)
            s_vendorsCount = 1;

        if (s_siegeIntervalMin < 1)
            s_siegeIntervalMin = 1;
        if (s_siegeIntervalMax < 1)
            s_siegeIntervalMax = 1;
        if (s_siegeIntervalMin > s_siegeIntervalMax)
        {
            uint32 tmp = s_siegeIntervalMin;
            s_siegeIntervalMin = s_siegeIntervalMax;
            s_siegeIntervalMax = tmp;
        }

        if (s_siegeAttackerMin < 2)
            s_siegeAttackerMin = 2;
        if (s_siegeAttackerMax < 2)
            s_siegeAttackerMax = 2;
        if (s_siegeAttackerMin > s_siegeAttackerMax)
        {
            uint32 tmp = s_siegeAttackerMin;
            s_siegeAttackerMin = s_siegeAttackerMax;
            s_siegeAttackerMax = tmp;
        }

        if (s_siegeDurationMin < 1)
            s_siegeDurationMin = 1;
        if (s_petsSummonSec < 5)
            s_petsSummonSec = 5;
    }

    bool IsEnabled()                    { return s_enabled; }
    uint32 GetBotAccountId()            { return s_botAccountId; }
    uint32 GetMaxBotsPerPlayer()        { return s_maxBots; }
    bool IsAutoSpawnEnabled()           { return s_autoSpawn; }
    uint32 GetBotsPerLogin()            { return s_botsPerLogin; }
    uint32 GetUpdateIntervalMs()        { return s_updateMs; }
    bool IsCombatEnabled()              { return s_combat; }
    float GetFollowDistance()           { return s_followDist; }
    bool IsCommandAvailableToAll()      { return s_cmdAll; }
    std::string const& GetDefaultRaceClass() { return s_defaultRC; }
    bool IsRandomRaceClassEnabled()        { return s_randomRC; }

    bool IsUniqueAccountEnabled()          { return s_uniqueAccount; }
    std::string const& GetUniqueAccountPrefix() { return s_uniquePrefix; }
    uint32 GetUniqueAccountStartId()       { return s_uniqueStartId; }

    bool    IsWorldBotsEnabled()          { return s_worldEnabled; }
    uint32  GetWorldBotsCount()           { return s_worldCount; }
    uint8   GetWorldBotsSpawnMode()       { return s_worldSpawnMode; }
    uint32  GetWorldBotsRespawnSeconds()  { return s_worldRespawnSec; }
    bool    IsWorldBotsSpellsEnabled()    { return s_worldSpells; }
    bool    IsWorldBotsChatEnabled()      { return s_worldChat; }
    uint32  GetBattleCryChance()          { return s_battleCryChance; }
    uint32  GetIdleChatChance()           { return s_idleChatChance; }

    uint32  GetWorldBotLevel()            { return s_worldBotLevel; }
    uint32  GetWorldBotLevelMin()         { return s_worldBotLevelMin; }
    uint32  GetWorldBotLevelMax()         { return s_worldBotLevelMax; }
    uint32  GetSyncOwnerLevel()           { return s_syncOwnerLevel; }
    uint32  GetFleeHealthPct()            { return s_fleeHealthPct; }
    uint32  GetHealThresholdPct()         { return s_healThresholdPct; }
    bool    IsBuffsEnabled()              { return s_useBuffs; }

    uint8   GetNamesMode()                { return s_namesMode; }

    float   GetPatrolRadius()             { return s_patrolRadius; }
    bool    IsLevelingEnabled()           { return s_leveling; }
    bool    IsProgressionEnabled()        { return s_progressionEnabled; }
    uint32  GetProgressionMaxLevel()      { return s_progressionMaxLevel; }
    float   GetProgressionXpRate()        { return s_progressionXpRate; }
    bool    IsProgressionAutoLevelEnabled() { return s_progressionAutoLevel; }
    uint32  GetProgressionAutoLevelIntervalSec() { return s_progressionAutoSec; }
    bool    IsWorldBotsPvPEnabled()       { return s_worldPvP; }
    uint32  GetWorldBotsPvPShare()        { return s_worldPvPShare; }
    bool    IsEndgameBotsEnabled()        { return s_endgameEnabled; }
    uint32  GetEndgameBotsCount()         { return s_endgameCount; }
    uint32  GetEndgameLevelMin()          { return s_endgameLevelMin; }
    uint32  GetEndgameLevelMax()          { return s_endgameLevelMax; }
    uint32  GetWorldBotsSpawnIntervalSec()  { return s_spawnIntervalSec; }
    uint32  GetWorldBotsSpawnStartupCount() { return s_spawnStartupCount; }
    uint32  GetWorldBotsProfilesNewPercent() { return s_profilesNewPercent; }
    uint32  GetEliteChance()              { return s_eliteChance; }
    bool    IsDuelsEnabled()              { return s_duels; }
    uint32  GetDuelChance()               { return s_duelChance; }
    bool    IsGroupsEnabled()             { return s_groups; }
    uint32  GetGroupMaxSize()             { return s_groupMaxSize; }
    bool    IsVendorsEnabled()            { return s_vendors; }
    uint32  GetVendorsCount()             { return s_vendorsCount; }
    bool    IsTerritoryEnabled()          { return s_territory; }
    uint32  GetTerritoryFlipMinutes()     { return s_territoryFlipMin; }

    bool    IsGuildEnabled()              { return s_guildEnabled; }
    std::string const& GetAllianceGuildName() { return s_guildAllianceName; }
    std::string const& GetHordeGuildName()     { return s_guildHordeName; }
    uint32  GetGuildXpPerKill()           { return s_guildXpPerKill; }
    uint32  GetGuildXpPerLevelBase()      { return s_guildXpBase; }
    uint32  GetGuildMaxLevel()            { return s_guildMaxLevel; }

    bool    IsSiegeEnabled()              { return s_siegeEnabled; }
    uint32  GetSiegeIntervalMin()         { return s_siegeIntervalMin; }
    uint32  GetSiegeIntervalMax()         { return s_siegeIntervalMax; }
    uint32  GetSiegeAttackerMin()         { return s_siegeAttackerMin; }
    uint32  GetSiegeAttackerMax()         { return s_siegeAttackerMax; }
    uint32  GetSiegeDurationMinutes()     { return s_siegeDurationMin; }

    bool    IsPetsEnabled()               { return s_petsEnabled; }
    uint32  GetPetsSummonInterval()       { return s_petsSummonSec; }
}
