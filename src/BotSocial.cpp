/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotSocial.cpp
 *
 * Боты замечают друг друга: приветствуют союзных, обмениваются
 * угрозами с враждебными, изредка устраивают дуэли 1v1.
 */

#include "BotSocial.h"
#include "BotMgr.h"
#include "Config_Bots.h"
#include "WorldBots.h"

#include "Cell.h"
#include "CellImpl.h"
#include "Chat.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "World.h"

#include <map>
#include <string>
#include <vector>

namespace
{
    // Реплики при встрече (фракция своя / враждебная).
    std::vector<std::string> const FriendlyGreetings =
    {
        "Well met, friend!",
        "Good to see an ally here.",
        "The streets are dangerous. Stay sharp!",
        "May the Light watch over you.",
        "For the %FACTION%!"
    };

    std::vector<std::string> const HostileTaunts =
    {
        "You're trespassing on %FACTION% lands!",
        "Get lost, %ENEMY%!",
        "I've killed better than you before.",
        "Keep your distance, dog.",
        "%ENEMY% scum!"
    };

    // Короткие диалоги бот-бот (пары фраз).
    std::vector<std::pair<std::string, std::string>> const Dialogues =
    {
        { "Need a hand with anything?", "All good here. Thanks for asking!" },
        { "Quiet day, huh?", "Enjoy it while it lasts." },
        { "Seen any action lately?", "A few gnolls. Nothing serious." },
        { "Heading anywhere?", "Just patrolling. You?" },
        { "That's a fine weapon you carry.", "It has served me well." },
        { "Rumor says the road east is unsafe.", "Then we should be extra careful." }
    };

    // Реплики дуэли.
    std::vector<std::string> const DuelChallenges =
    {
        "You there! Prove your worth in a duel!",
        "Let's settle this like warriors!",
        "I challenge you! Right here, right now!"
    };

    uint32 const SocialCheckIntervalMs = 15000;
    float const SocialRange = 15.0f;
    float const DuelRange = 10.0f;

    std::string ReplaceTokens(std::string text, TeamId team)
    {
        std::string faction = (team == TEAM_HORDE) ? "the Horde" : "the Alliance";
        std::string enemy = (team == TEAM_HORDE) ? "Alliance" : "Horde";

        size_t pos = text.find("%FACTION%");
        while (pos != std::string::npos)
        {
            text.replace(pos, 9, faction);
            pos = text.find("%FACTION%", pos);
        }
        pos = text.find("%ENEMY%");
        while (pos != std::string::npos)
        {
            text.replace(pos, 7, enemy);
            pos = text.find("%ENEMY%", pos);
        }
        return text;
    }

    void BotSay(Player* bot, std::string const& text)
    {
        if (bot)
            bot->Say(text, LANG_UNIVERSAL);
    }
}

namespace AiBotsSocial
{
    void Greet(Player* bot, Player* other)
    {
        if (!bot || !other)
            return;

        if (bot->IsFriendlyTo(other))
        {
            std::string text = FriendlyGreetings[urand(0, FriendlyGreetings.size() - 1)];
            BotSay(bot, ReplaceTokens(text, bot->GetTeamId() == TEAM_HORDE ? TEAM_HORDE : TEAM_ALLIANCE));
        }
        else
        {
            std::string text = HostileTaunts[urand(0, HostileTaunts.size() - 1)];
            BotSay(bot, ReplaceTokens(text, bot->GetTeamId() == TEAM_HORDE ? TEAM_HORDE : TEAM_ALLIANCE));
        }
    }

    bool TryStartDuel(Player* bot, Player* other)
    {
        if (!bot || !other || !AiBotsConfig::IsDuelsEnabled())
            return false;

        if (bot->GetDistance(other) > DuelRange)
            return false;

        if (!roll_chance_i(AiBotsConfig::GetDuelChance()))
            return false;

        BotSay(bot, DuelChallenges[urand(0, DuelChallenges.size() - 1)]);
        // Метод Player::Duel() отсутствует в этом ядре (TrinityCore API),
        // поэтому реальная дуэль 1v1 не инициируется.
        return true;
    }

    void Update(uint32 diff)
    {
        if (!AiBotsConfig::IsWorldBotsEnabled() || !AiBotsConfig::IsWorldBotsChatEnabled())
            return;

        // Болтовня — раз в 30 секунд, а не каждый тик мира.
        static uint32 s_socialTimer = 0;
        s_socialTimer += diff;
        if (s_socialTimer < 30000)
            return;
        s_socialTimer = 0;

        // Периодическая проверка выполняется снаружи (тикер), здесь —
        // лёгкий проход по ботам текущей карты через прямой перебор.
        // Диалоги: два бота одной фракции рядом иногда коротко беседуют.
        if (roll_chance_i(30))
        {
            // Диалоги ищут пару среди всех ботов: владельцев и автономных.
            std::vector<Player*> bots;
            for (auto const& pair : sBotMgr->GetAllBots())
                if (Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(pair.first)))
                    bots.push_back(bot);

            std::vector<Player*> worldBots;
            sWorldBots->CollectOnlineBots(worldBots);
            bots.insert(bots.end(), worldBots.begin(), worldBots.end());

            Player* first = nullptr;
            Player* second = nullptr;

            for (Player* bot : bots)
            {
                if (!bot->IsInWorld() || !bot->IsAlive() || bot->IsInCombat())
                    continue;

                if (!first)
                    first = bot;
                else if (first->GetMapId() == bot->GetMapId() &&
                         first->GetDistance(bot) <= SocialRange &&
                         first->IsFriendlyTo(bot))
                {
                    second = bot;
                    break;
                }
            }

            if (first && second)
            {
                auto const& dialogue = Dialogues[urand(0, Dialogues.size() - 1)];
                BotSay(first, dialogue.first);
                BotSay(second, dialogue.second);
            }
        }
    }
}
