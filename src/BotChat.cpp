/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotChat.cpp
 *
 * Боты изредка пишут в мировой канал "World" и в чат гильдии.
 * Канал создаётся при первом входе бота, если его ещё нет.
 */

#include "BotChat.h"

#include "Channel.h"
#include "ChannelMgr.h"
#include "Guild.h"
#include "Log.h"
#include "Player.h"
#include "SharedDefines.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
    char const* WorldChannelName = "World";
    uint32 const ChatIntervalMs = 180000; // раз в 3 минуты

    std::vector<std::string> const& WorldPhrases()
    {
        static std::vector<std::string> const v =
        {
            "Кто на охоту? Пишите в личку.",
            "Есть кто живой? Отзовитесь.",
            "Ищу группу в подземелье, есть лекарь.",
            "Продам кожу и руду, недорого.",
            "Кто видел орду у переправы?",
            "Собираю на элитного моба, нужен танк.",
            "Народ, где тут ближайший торговец?",
            "Осторожно на дороге — патрули."
        };
        return v;
    }

    std::vector<std::string> const& GuildPhrases()
    {
        static std::vector<std::string> const v =
        {
            "Всем привет. Кто онлайн?",
            "Нужна помощь с заданием, кто свободен?",
            "Есть лишние сумки? Куплю.",
            "Собираюсь в подземелье, есть желающие?",
            "Осторожно, в округе враги."
        };
        return v;
    }

    std::string Pick(std::vector<std::string> const& list)
    {
        return list.empty() ? std::string() : list[urand(0, list.size() - 1)];
    }

    std::map<ObjectGuid::LowType, uint32> s_timers;
    std::set<ObjectGuid::LowType> s_joinedWorld;
}

namespace AiBotsChat
{
    void Update(Player* bot, uint32 diff)
    {
        if (!bot || !bot->IsAlive() || !bot->IsInWorld() || bot->IsInCombat())
            return;

        uint32& timer = s_timers[bot->GetGUID().GetCounter()];
        timer += diff;
        if (timer < ChatIntervalMs)
            return;
        timer = 0;

        // Половина реплик — в мир, половина — в гильдию (если она есть).
        if (roll_chance_i(50))
        {
            ChannelMgr* mgr = ChannelMgr::forTeam(bot->GetTeamId());
            if (!mgr)
                return;

            Channel* channel = mgr->GetChannel(WorldChannelName, bot, false);
            if (!channel)
                channel = mgr->GetJoinChannel(WorldChannelName, 0);
            if (!channel)
                return;

            ObjectGuid::LowType guid = bot->GetGUID().GetCounter();
            if (!s_joinedWorld.count(guid))
            {
                channel->JoinChannel(bot, "");
                s_joinedWorld.insert(guid);
            }

            std::string text = Pick(WorldPhrases());
            channel->Say(bot->GetGUID(), text, LANG_UNIVERSAL);
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} написал в канал {}: '{}'.",
                bot->GetName(), WorldChannelName, text);
            return;
        }

        if (Guild* guild = bot->GetGuild())
        {
            std::string text = Pick(GuildPhrases());
            guild->BroadcastToGuild(bot->GetSession(), false, text, LANG_UNIVERSAL);
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} написал в чат гильдии: '{}'.",
                bot->GetName(), text);
        }
    }
}
