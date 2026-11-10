/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotDialogue.cpp
 *
 * Диалоги ботов: разбор ключевых слов в сообщении игрока и ответ
 * с учётом состояния бота (бой, здоровье), плюс обмен репликами
 * между двумя ботами рядом.
 */

#include "BotDialogue.h"
#include "BotMgr.h"

#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "Player.h"
#include "SharedDefines.h"

#include <list>
#include <map>
#include <string>
#include <vector>

namespace
{
    // Нижний регистр для ASCII и кириллицы в UTF-8.
    std::string ToLower(std::string text)
    {
        for (size_t i = 0; i + 1 < text.size();)
        {
            unsigned char c = text[i];
            unsigned char d = text[i + 1];

            if (c == 0xD0)
            {
                if (d == 0x81) { text[i] = char(0xD1); text[i + 1] = char(0x91); i += 2; continue; } // Ё -> ё
                if (d >= 0x90 && d <= 0x9F) { text[i + 1] = char(d + 0x20); i += 2; continue; }      // А-П
                if (d >= 0xA0 && d <= 0xAF) { text[i] = char(0xD1); text[i + 1] = char(d - 0x20); i += 2; continue; } // Р-Я
                i += 2;
                continue;
            }

            if (c == 0xD1)
            {
                if (d >= 0x80 && d <= 0x8F) { text[i + 1] = char(d + 0x20); i += 2; continue; }      // Р-Я (нижний)
                i += 2;
                continue;
            }

            if (c >= 'A' && c <= 'Z')
                text[i] = char(c + 32);

            ++i;
        }

        return text;
    }

    bool Contains(std::string const& haystack, char const* needle)
    {
        return haystack.find(needle) != std::string::npos;
    }

    std::vector<std::string> const& Greetings()
    {
        static std::vector<std::string> const v =
        {
            "Привет, путник.",
            "Здравствуй. Нынче неспокойно.",
            "Приветствую. Что нужно?",
            "Салют. Береги себя на дороге."
        };
        return v;
    }

    std::vector<std::string> const& Neutral()
    {
        static std::vector<std::string> const v =
        {
            "Как скажешь.",
            "Понял тебя.",
            "Хм, посмотрим.",
            "Не уверен, но ладно.",
            "Держись рядом."
        };
        return v;
    }

    std::vector<std::string> const& Farewells()
    {
        static std::vector<std::string> const v =
        {
            "Удачи на дороге.",
            "До встречи.",
            "Храни тебя Свет.",
            "Будь осторожен."
        };
        return v;
    }

    std::string Pick(std::vector<std::string> const& list)
    {
        return list.empty() ? std::string() : list[urand(0, list.size() - 1)];
    }

    bool IsBot(Player* player)
    {
        return player && sBotMgr && sBotMgr->IsBot(player->GetGUID().GetCounter());
    }

    Player* FindNearbyBot(Player* origin, float range)
    {
        std::list<Player*> players;
        Acore::AnyPlayerInObjectRangeCheck check(origin, range);
        Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(origin, players, check);
        TypeContainerVisitor<Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck>, GridTypeMapContainer> visitor(searcher);
        Cell cell(Acore::ComputeCellCoord(origin->GetPositionX(), origin->GetPositionY()));
        cell.Visit(Acore::ComputeCellCoord(origin->GetPositionX(), origin->GetPositionY()), visitor, *origin->GetMap(), *origin, range);

        Player* best = nullptr;
        float bestDist = range;
        for (Player* player : players)
        {
            if (player == origin || !player->IsAlive() || !player->IsInWorld() || !IsBot(player))
                continue;

            float dist = origin->GetDistance(player);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = player;
            }
        }
        return best;
    }

    struct DialoguePair
    {
        std::string question;
        std::string answer;
    };

    std::vector<DialoguePair> const& DialoguePairs()
    {
        static std::vector<DialoguePair> const v =
        {
            { "Видел, что на дороге творится?", "Видел. Лучше держаться вместе." },
            { "Куда путь держишь?", "К ближайшему лагерю, передохнуть." },
            { "Готов к драке?", "Всегда готов. Только бы не с пустыми руками." },
            { "Ты давно здесь?", "С рассвета. Места тут неспокойные." },
            { "Слыхал про осаду?", "Слыхал. Города нынче не в безопасности." },
            { "Много заработал на заданиях?", "На хлеб хватает. Остальное — на снаряжение." },
            { "Не боишься один ходить?", "Боюсь. Потому и держу оружие наготове." },
            { "Что скажешь о местных?", "Народ разный. Но зла никто не желает." }
        };
        return v;
    }
}

namespace AiBotsDialogue
{
    std::string ReplyToPlayer(Player* bot, Player* /*sender*/, std::string const& message)
    {
        if (!bot || !bot->IsAlive())
            return Pick(Neutral());

        if (bot->IsInCombat() && roll_chance_i(40))
            return "Не сейчас — бой!";

        if (bot->GetHealthPct() < 35.0f)
            return "Раны бы подлечить…";

        std::string const text = ToLower(message);

        if (Contains(text, "привет") || Contains(text, "здрав") || Contains(text, "хай") || Contains(text, "салют"))
            return Pick(Greetings());

        if (Contains(text, "как дела") || Contains(text, "как ты") || Contains(text, "как жизнь"))
            return "Нормально. Тут неспокойно, но жить можно.";

        if (Contains(text, "помог") || Contains(text, "помощ") || Contains(text, "подсоб"))
            return "Могу подсобить, если враг рядом. Не отставай.";

        if (Contains(text, "групп") || Contains(text, "отряд") || Contains(text, "вместе"))
            return "Я не против. Собери группу — пойду с тобой.";

        if (Contains(text, "дуэл") || Contains(text, "пвп") || Contains(text, "драт") || Contains(text, "бой"))
            return "Давай, если не боишься. Только без обид потом.";

        if (Contains(text, "торгов") || Contains(text, "прода") || Contains(text, "куп"))
            return "Есть кое-что на продажу. Глянь у торговцев.";

        if (Contains(text, "квест") || Contains(text, "задани"))
            return "Ищу работу по силам. Что нужно сделать?";

        if (Contains(text, "где") || Contains(text, "куда"))
            return "Смотри по сторонам — тут каждый холм с сюрпризом.";

        if (Contains(text, "пока") || Contains(text, "бай") || Contains(text, "прощай"))
            return Pick(Farewells());

        if (Contains(text, "спасиб") || Contains(text, "благодар"))
            return "Не за что. Обращайся.";

        return Pick(Neutral());
    }

    void OnPlayerSaid(Player* speaker, uint32 type, std::string const& message)
    {
        if (!speaker || (type != CHAT_MSG_SAY && type != CHAT_MSG_YELL))
            return;
        if (IsBot(speaker))
            return;

        Player* bot = FindNearbyBot(speaker, 20.0f);
        if (!bot || bot->IsInCombat() || !roll_chance_i(50))
            return;

        std::string reply = ReplyToPlayer(bot, speaker, message);
        bot->Say(reply, LANG_UNIVERSAL);
        LOG_INFO("server.loading", ">> mod-ai-bots: бот {} ответил игроку {}: '{}'.",
            bot->GetName(), speaker->GetName(), reply);
    }

    void Update(Player* bot, uint32 diff)
    {
        if (!bot || !bot->IsAlive() || !bot->IsInWorld() || bot->IsInCombat())
            return;

        static std::map<ObjectGuid::LowType, uint32> s_timers;
        uint32& timer = s_timers[bot->GetGUID().GetCounter()];
        timer += diff;
        if (timer < 90000)
            return;
        timer = 0;

        if (!roll_chance_i(40))
            return;

        Player* other = FindNearbyBot(bot, 15.0f);
        if (!other || other->IsInCombat())
            return;

        auto const& pairs = DialoguePairs();
        DialoguePair const& pair = pairs[urand(0, pairs.size() - 1)];

        bot->Say(pair.question, LANG_UNIVERSAL);
        other->Say(pair.answer, LANG_UNIVERSAL);

        LOG_INFO("server.loading", ">> mod-ai-bots: диалог {} <-> {}: '{}' / '{}'.",
            bot->GetName(), other->GetName(), pair.question, pair.answer);
    }
}
