/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotBattleground.cpp
 *
 * Автономные боты раз в 5 минут встают в очередь поля сражений
 * (WSG/AB/AV по уровню), принимают приглашение и входят в бой.
 * Дальний телепорт завершаем вручную — у бота нет клиента.
 */

#include "BotBattleground.h"
#include "BotAI.h"
#include "BotMgr.h"

#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "DBCStores.h"
#include "Log.h"
#include "Opcodes.h"
#include "Player.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <map>
#include <set>
#include <utility>

namespace
{
    std::map<ObjectGuid::LowType, uint32> s_queueTimers;
    std::set<ObjectGuid::LowType> s_inBgLogged;
    std::map<std::pair<ObjectGuid::LowType, BattlegroundQueueTypeId>, bool> s_acceptLogged;

    BattlegroundTypeId PickBgType(Player* bot)
    {
        if (bot->GetLevel() >= 51)
            return BATTLEGROUND_AV;
        if (bot->GetLevel() >= 20)
            return BATTLEGROUND_AB;
        return BATTLEGROUND_WS;
    }

    void TryQueue(Player* bot, WorldSession* session, uint32 diff)
    {
        if (bot->GetLevel() < 10)
            return;
        if (bot->InBattlegroundQueue() || bot->GetBattlegroundId())
            return;

        uint32& timer = s_queueTimers[bot->GetGUID().GetCounter()];
        timer += diff;
        if (timer < 300000)
            return;
        timer = 0;

        BattlegroundTypeId bgTypeId = PickBgType(bot);

        WorldPacket data(CMSG_BATTLEMASTER_JOIN);
        data << ObjectGuid::Empty;      // очередь из интерфейса, без NPC
        data << uint32(bgTypeId);
        data << uint32(0);              // первое доступное
        data << uint8(0);               // не группой

        session->HandleBattlemasterJoinOpcode(data);

        if (bot->InBattlegroundQueue())
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} (уровень {}) встал в очередь BG {}.",
                bot->GetName(), uint32(bot->GetLevel()), uint32(bgTypeId));
    }

    void TryAcceptInvite(Player* bot, WorldSession* session)
    {
        for (uint8 slot = 0; slot < PLAYER_MAX_BATTLEGROUND_QUEUES; ++slot)
        {
            BattlegroundQueueTypeId queueTypeId = bot->GetBattlegroundQueueTypeId(slot);
            if (queueTypeId == BATTLEGROUND_QUEUE_NONE || !bot->IsInvitedForBattlegroundQueueType(queueTypeId))
                continue;

            auto key = std::make_pair(bot->GetGUID().GetCounter(), queueTypeId);
            if (s_acceptLogged[key])
                continue;

            // Ядро отказывает во входе, если игрок в бою — снимаем бой перед портом.
            if (bot->IsInCombat())
                bot->CombatStop(true);

            WorldPacket data(CMSG_BATTLEFIELD_PORT);
            data << uint8(BattlegroundMgr::BGArenaType(queueTypeId));
            data << uint8(0);
            data << uint32(BattlegroundMgr::BGTemplateId(queueTypeId));
            data << uint16(0);
            data << uint8(1); // action: enter battle

            session->HandleBattleFieldPortOpcode(data);
            s_acceptLogged[key] = true;
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} принял приглашение BG {}.",
                bot->GetName(), uint32(BattlegroundMgr::BGTemplateId(queueTypeId)));
            return;
        }
    }
}

namespace AiBotsBattleground
{
    void Update(Player* bot, uint32 diff)
    {
        if (!bot || !bot->IsInWorld())
            return;

        WorldSession* session = bot->GetSession();
        if (!session)
            return;

        ObjectGuid::LowType guid = bot->GetGUID().GetCounter();

        // Сбрасываем отметки, когда бот вне очереди и вне поля боя:
        // иначе после первого принятого приглашения он больше не примет
        // приглашение на это же поле сражений.
        if (!bot->InBattlegroundQueue() && !bot->GetBattlegroundId() && !bot->IsBeingTeleported())
        {
            for (auto it = s_acceptLogged.begin(); it != s_acceptLogged.end(); )
            {
                if (it->first.first == guid)
                    it = s_acceptLogged.erase(it);
                else
                    ++it;
            }

            s_inBgLogged.erase(guid);
        }

        // Подтверждение входа: бот действительно на поле боя.
        if (bot->InBattleground() && !s_inBgLogged.count(bot->GetGUID().GetCounter()))
        {
            s_inBgLogged.insert(bot->GetGUID().GetCounter());
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} на поле боя (карта {}).",
                bot->GetName(), bot->GetMapId());
        }

        // Завершаем дальний телепорт (вход/выход из BG).
        if (bot->IsBeingTeleportedFar())
        {
            session->HandleMoveWorldportAck();
            return;
        }

        TryAcceptInvite(bot, session);

        BotAI* ai = sBotMgr->GetBotAI(bot->GetGUID().GetCounter());
        if (!ai || !ai->IsAutonomous())
            return;

        TryQueue(bot, session, diff);
    }
}
