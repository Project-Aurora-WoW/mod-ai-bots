/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotPersonality.cpp
 *
 * Пять личностей: Aggressive, Calm, Jester, Grump, Hero.
 * Выбор личности: guid % 5 — бот всегда сохраняет свой характер.
 */

#include "BotPersonality.h"
#include "BotChatPhrases.h"

namespace
{
    void AppendPhrases(std::vector<std::string>& dst, std::vector<std::string> const& src)
    {
        dst.insert(dst.end(), src.begin(), src.end());
    }

    std::vector<BotPersonality> BuildPersonalities()
    {
        std::vector<BotPersonality> list =
    {
        {
            "Aggressive",
            { "DIE!", "I'll crush you!", "Blood for blood!", "No mercy!", "Feel my wrath!" },
            { "As expected.", "Too easy.", "Next victim!", "Fear me!" },
            { "Anyone wants a fight?!", "So bored... need action!", "Where's a good brawl?" },
            { "I'm not running... yet!", "This isn't over!", "Grrr... fall back!" },
            { "Healing! Don't interrupt!", "Pain is temporary!" }
        },
        {
            "Calm",
            { "Stand down or be stopped.", "I will defend this place.", "This ends now.", "Let us resolve this." },
            { "Balance restored.", "It is done.", "May you find peace.", "Well fought." },
            { "The wind feels calm today.", "Peace be with you, traveler.", "Patience is a virtue.", "I walk my own path." },
            { "I shall withdraw wisely.", "Discretion is the better part of valor." },
            { "The Light mends what is broken.", "Rest now, wounds." }
        },
        {
            "Jester",
            { "Tag! You're it!", "Did it hurt when you fell from heaven? Too bad!", "Free hugs! *swings axe*", "Boo!" },
            { "Do I get a prize?", "That was fun! Again!", "Somebody call a healer... oh wait, that's me!", "10/10 would fight again." },
            { "Dance with me!", "Know any good jokes?", "My horse has more manners than you!", "Shiny things everywhere!" },
            { "Retreat? I prefer 'tactical giggling'!", "This game is too rough!" },
            { "Patching myself up!", "Aaaand... fixed!" }
        },
        {
            "Grump",
            { "Get off my lawn!", "Bah! Trouble again?", "You again?!", "Just once I wanted a quiet day..." },
            { "Hmph. Took long enough.", "Finally. Now leave me alone.", "Was that all?" },
            { "Kids these days...", "Back in MY day...", "Don't bother me.", "Everything was better before." },
            { "Fine, fine, I'm going!", "Old bones need a break..." },
            { "Bandages, always out of bandages...", "Give me a minute here!" }
        },
        {
            "Hero",
            { "Evil will not pass!", "For justice!", "I will protect the weak!", "Stand together, friends!", "This is our land!" },
            { "Justice served!", "The light guides us!", "Another victory for good!", "Heroes never yield!" },
            { "Where are the ones who need help?", "A hero's work is never done.", "Anyone in trouble?", "Adventures await!" },
            { "I'll return stronger!", "Even heroes retreat today...", "This isn't my final stand!" },
            { "The Light heals the righteous!", "Stay strong, I've got this!" }
        }
        };

        for (BotPersonality& p : list)
        {
            AppendPhrases(p.battleCries, BotChatPhrases::BattleCries());
            AppendPhrases(p.victory, BotChatPhrases::Victory());
            AppendPhrases(p.idle, BotChatPhrases::Idle());
        }

        return list;
    }

    std::vector<BotPersonality> const Personalities = BuildPersonalities();
}

std::vector<BotPersonality> const& AiBotsPersonality::GetAll()
{
    return Personalities;
}

BotPersonality const& AiBotsPersonality::GetForGuid(ObjectGuid::LowType guid)
{
    return Personalities[guid % Personalities.size()];
}
