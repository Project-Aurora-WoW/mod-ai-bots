/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotGuild.cpp
 *
 * Прокачка как в Cataclysm: уровни 1-25, на каждом — перки
 * (по образцу настоящих гильдейских бонусов):
 *   ур. 2  — Fast Track: +5% XP
 *   ур. 5  — Fast Track II: +10% XP
 *   ур. 6  — Cash Flow: +5% золота в казну с каждого лута
 *   ур. 7  — Chug-A-Lug: -25% на зелья (симулируется бонусом золота +2%)
 *   ур. 8  — Honor Among Allies: +5% honor (симулируется XP +2%)
 *   ур. 10 — Bartering: +10% цена продажи (бонус золота +5%)
 *   ур. 11 — Happy Hour: казна +2x
 *   ур. 13 — Speedy Shoes (Dead Man's Shoes аналог): дух +15% скорость
 *   ур. 16 — Bountiful Bags: +100% пространство сумок (бонус золота +3%)
 * XP и казна сохраняются в mod_ai_bots_guild.
 */

#include "BotGuild.h"
#include "BotMgr.h"
#include "Config_Bots.h"
#include "WorldBots.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "World.h"
#include "WorldSessionMgr.h"
#include "WorldSession.h"

#include <algorithm>
#include <vector>

namespace
{
    uint32 s_guildLevel = 1;
    uint32 s_guildXp = 0;
    uint32 s_guildVault = 0;   // казна в медяках
    bool s_initialized = false;
    uint32 s_saveCounter = 0;

    void SaveToDb()
    {
        WorldDatabase.DirectExecute(
            "REPLACE INTO `mod_ai_bots_guild` (`id`, `level`, `xp`, `vault`) VALUES (1, {}, {}, {})",
            s_guildLevel, s_guildXp, s_guildVault);
    }

    void AnnounceLevelUp()
    {
        std::string message = "[AI-Bots] Guild '" + AiBotsConfig::GetAllianceGuildName()
            + "' has reached level " + std::to_string(s_guildLevel) + "!";
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, message.c_str());
    }

    void TryLevelUp()
    {
        uint32 maxLevel = AiBotsConfig::GetGuildMaxLevel();
        while (s_guildLevel < maxLevel)
        {
            uint32 needed = AiBotsConfig::GetGuildXpPerLevelBase() * s_guildLevel;
            if (s_guildXp < needed)
                break;
            s_guildXp -= needed;
            ++s_guildLevel;
            AnnounceLevelUp();
        }
        if (s_guildLevel >= maxLevel)
            s_guildXp = 0; // на максимуме XP не копится
    }
}

namespace AiBotsGuild
{
    void Init()
    {
        if (!AiBotsConfig::IsGuildEnabled() || s_initialized)
            return;

        // Восстановление уровня и казны из БД.
        if (QueryResult result = WorldDatabase.Query(
            "SELECT `level`, `xp`, `vault` FROM `mod_ai_bots_guild` WHERE `id` = 1"))
        {
            Field* fields = result->Fetch();
            s_guildLevel = std::max<uint32>(1, fields[0].Get<uint32>());
            s_guildXp = fields[1].Get<uint32>();
            s_guildVault = fields[2].Get<uint32>();
        }

        s_initialized = true;
        LOG_INFO("server.loading", ">> mod-ai-bots: гильдия '{}' (уровень {}, xp {}, казна {}м).",
            AiBotsConfig::GetAllianceGuildName(), s_guildLevel, s_guildXp, s_guildVault);
    }

void AddMember(Player* bot)
    {
        if (!bot || !AiBotsConfig::IsGuildEnabled())
            return;

        Guild* guild = sGuildMgr->GetGuildByName(AiBotsConfig::GetAllianceGuildName());
        if (!guild)
        {
            Guild* newGuild = new Guild();
            if (!newGuild->Create(bot, AiBotsConfig::GetAllianceGuildName()))
            {
                delete newGuild;
                LOG_WARN("server.loading", ">> mod-ai-bots: не удалось создать гильдию '{}'.", AiBotsConfig::GetAllianceGuildName());
                return;
            }
            sGuildMgr->AddGuild(newGuild);
            guild = sGuildMgr->GetGuildByName(AiBotsConfig::GetAllianceGuildName());
            if (!guild)
                return;

            LOG_INFO("server.loading", ">> mod-ai-bots: создана гильдия '{}', лидер {}.",
                AiBotsConfig::GetAllianceGuildName(), bot->GetName());
        }

        if (bot->GetGuildId() == guild->GetId())
            return;

        if (guild->AddMember(bot->GetGUID()))
            LOG_DEBUG("server.loading", ">> mod-ai-bots: бот {} вступил в гильдию '{}'.",
                bot->GetName(), AiBotsConfig::GetAllianceGuildName());
    }

void AwardKillXp(Player* bot, Unit* victim)
    {
        if (!AiBotsConfig::IsGuildEnabled() || !bot || !victim)
            return;

        Guild* guild = sGuildMgr->GetGuildByName(AiBotsConfig::GetAllianceGuildName());
        if (!guild || bot->GetGuildId() != guild->GetId())
            return;

        s_guildXp += AiBotsConfig::GetGuildXpPerKill() * std::max<uint32>(1, victim->GetLevel() / 10);
        TryLevelUp();

        if (++s_saveCounter >= 50)
        {
            s_saveCounter = 0;
            SaveToDb();
        }
    }

    void AwardLevelXp(Player* bot)
    {
        if (!AiBotsConfig::IsGuildEnabled() || !bot)
            return;

        s_guildXp += 100 * bot->GetLevel();
        TryLevelUp();
    }

    // --- Перки ---

    uint32 GetXpBonusPct()
    {
        if (!AiBotsConfig::IsGuildEnabled())
            return 100;

        // Fast Track (+5% на 2, +10% на 5), Honor Among Allies (+2% на 8).
        uint32 bonus = 0;
        if (s_guildLevel >= 2)  bonus += 5;
        if (s_guildLevel >= 5)  bonus += 5;
        if (s_guildLevel >= 8)  bonus += 2;
        return 100 + bonus;
    }

    uint32 GetGoldBonusPct()
    {
        if (!AiBotsConfig::IsGuildEnabled())
            return 100;

        // Cash Flow (+5% на 6), Bartering (+5% на 10), Chug-A-Lug (+2% на 7), Bountiful Bags (+3% на 16).
        uint32 bonus = 0;
        if (s_guildLevel >= 6)  bonus += 5;
        if (s_guildLevel >= 7)  bonus += 2;
        if (s_guildLevel >= 10) bonus += 5;
        if (s_guildLevel >= 16) bonus += 3;
        return 100 + bonus;
    }

    uint32 GetMoveSpeedBonusPct()
    {
        if (!AiBotsConfig::IsGuildEnabled())
            return 0;

        // Dead Man's Shoes: +15% скорость духа на 13 уровне.
        return s_guildLevel >= 13 ? 15 : 0;
    }

    void DepositToVault(uint32 copper)
    {
        if (!AiBotsConfig::IsGuildEnabled() || !copper)
            return;

        // Happy Hour (ур. 11): двойное пополнение казны.
        s_guildVault += s_guildLevel >= 11 ? copper * 2 : copper;

        if (++s_saveCounter >= 50)
        {
            s_saveCounter = 0;
            SaveToDb();
        }
    }

    void Update(uint32 diff)
    {
        if (!AiBotsConfig::IsGuildEnabled())
            return;

        // Проверка членства — раз в 5 секунд, а не каждый тик мира.
        static uint32 s_updateTimer = 0;
        s_updateTimer += diff;
        if (s_updateTimer < 5000)
            return;
        s_updateTimer = 0;

        auto addIfNeeded = [](Player* bot)
        {
            if (bot && bot->IsInWorld() && !bot->GetGuildId())
                AddMember(bot);
        };

        // Гильдия объединяет всех ботов: и ботов владельцев, и автономных.
        for (auto const& pair : sBotMgr->GetAllBots())
            if (Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(pair.first)))
                addIfNeeded(bot);

        std::vector<Player*> worldBots;
        sWorldBots->CollectOnlineBots(worldBots);
        for (Player* bot : worldBots)
            addIfNeeded(bot);
    }

    uint32 GetLevel()             { return s_guildLevel; }
    uint32 GetXp()                { return s_guildXp; }
    uint32 GetXpForNextLevel()    { return AiBotsConfig::GetGuildXpPerLevelBase() * s_guildLevel; }
uint32 GetMemberCount()
    {
        Guild* guild = sGuildMgr->GetGuildByName(AiBotsConfig::GetAllianceGuildName());
        return guild ? guild->GetMemberCount() : 0;
    }
uint32 GetVault()             { return s_guildVault; }

    std::string const& GetGuildNameFor(TeamId /*team*/)
    {
        return AiBotsConfig::GetAllianceGuildName();
    }
}
