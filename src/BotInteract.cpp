/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotInteract.cpp
 *
 * Бот отвечает на шёпот игрока, принимает приглашение в группу,
 * подтверждает обмен (когда игрок уже подтвердил) и принимает дуэль.
 */

#include "BotInteract.h"
#include "BotDialogue.h"
#include "BotMgr.h"

#include "Group.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SharedDefines.h"
#include "TradeData.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <vector>

namespace
{
    bool IsBotGuid(ObjectGuid::LowType guid)
    {
        return sBotMgr && sBotMgr->IsBot(guid);
    }

    std::vector<std::string> const& Replies()
    {
        static std::vector<std::string> const replies =
        {
            "Привет! Чем помочь?",
            "Я на задании, но слушаю.",
            "Не сейчас — иду по делам.",
            "Следуй за мной, если хочешь.",
            "Чем могу помочь?"
        };
        return replies;
    }
}

namespace AiBotsInteract
{
    void OnPrivateChat(Player* sender, uint32 type, uint32 /*language*/, std::string& msg, Player* receiver)
    {
        if (!sender || !receiver || type != CHAT_MSG_WHISPER)
            return;
        if (IsBotGuid(sender->GetGUID().GetCounter()))
            return;
        if (!IsBotGuid(receiver->GetGUID().GetCounter()))
            return;

        std::string reply = AiBotsDialogue::ReplyToPlayer(receiver, sender, msg);
        receiver->Whisper(reply, LANG_UNIVERSAL, sender);
    }

    void Update(Player* bot)
    {
        if (!bot || !bot->IsInWorld())
            return;

        WorldSession* session = bot->GetSession();
        if (!session)
            return;

        // Приглашение в группу от игрока — принимаем.
        if (Group* invite = bot->GetGroupInvite())
        {
            if (!bot->GetGroup())
            {
                Player* leader = ObjectAccessor::FindConnectedPlayer(invite->GetLeaderGUID());
                if (leader && !IsBotGuid(leader->GetGUID().GetCounter()))
                {
                    WorldPacket data(CMSG_GROUP_ACCEPT);
                    data << uint32(0);
                    session->HandleGroupAcceptOpcode(data);
                    LOG_INFO("server.loading", ">> mod-ai-bots: бот {} принял приглашение в группу от {}.",
                        bot->GetName(), leader->GetName());
                }
            }
        }

        // Трейд: подтверждаем, когда игрок уже подтвердил обмен.
        if (TradeData* trade = bot->GetTradeData())
        {
            Player* trader = trade->GetTrader();
            if (trader && !IsBotGuid(trader->GetGUID().GetCounter()))
            {
                TradeData* traderData = trade->GetTraderData();
                if (!trade->IsAccepted() && traderData && traderData->IsAccepted())
                {
                    WorldPacket data(CMSG_ACCEPT_TRADE);
                    session->HandleAcceptTradeOpcode(data);
                    LOG_INFO("server.loading", ">> mod-ai-bots: бот {} подтвердил обмен с {}.",
                        bot->GetName(), trader->GetName());
                }
            }
        }

        // Дуэль: принимаем вызов от игрока.
        if (bot->duel && bot->duel->State == DUEL_STATE_CHALLENGED)
        {
            Player* opponent = bot->duel->Opponent;
            if (opponent && opponent->IsInWorld() && !IsBotGuid(opponent->GetGUID().GetCounter()))
            {
                WorldPacket data(CMSG_DUEL_ACCEPTED);
                data << opponent->GetGuidValue(PLAYER_DUEL_ARBITER);
                session->HandleDuelAcceptedOpcode(data);
                LOG_INFO("server.loading", ">> mod-ai-bots: бот {} принял дуэль с {}.",
                    bot->GetName(), opponent->GetName());
            }
        }
    }
}
