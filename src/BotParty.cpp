/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotParty.cpp
 *
 * Группа игрока: боты владельца добавляются в группу через ядро (Group),
 * а после смены карты владельцем переводятся к нему в тот же инстанс.
 * Переход выполняем отложенно на тике мира, чтобы не входить в
 * Map::AddPlayerToMap рекурсивно.
 */

#include "BotParty.h"
#include "BotMgr.h"

#include "GameTime.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Util.h"
#include "WorldSession.h"

#include <algorithm>
#include <vector>

namespace
{
    std::vector<ObjectGuid::LowType> s_pendingOwners;

    Player* FindBotPlayer(ObjectGuid::LowType guid)
    {
        return ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(guid));
    }

    void TeleportBotTo(Player* bot, Player* target)
    {
        if (!bot || !target || !bot->IsInWorld() || !bot->IsAlive())
            return;

        if (bot->GetMapId() == target->GetMapId() &&
            bot->GetInstanceId() == target->GetInstanceId())
            return;

        float x = target->GetPositionX() + frand(-2.0f, 2.0f);
        float y = target->GetPositionY() + frand(-2.0f, 2.0f);
        float z = target->GetPositionZ();
        float o = target->GetOrientation();

        if (!bot->TeleportTo(target->GetMapId(), x, y, z, o))
        {
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} не смог перейти к {} (map {}).",
                bot->GetName(), target->GetName(), target->GetMapId());
            return;
        }

        WorldSession* session = bot->GetSession();
        if (!session)
            return;

        // Дальний телепорт у бота некому подтвердить — завершаем сами.
        if (bot->IsBeingTeleportedFar())
        {
            session->HandleMoveWorldportAck();
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} перешёл за {} (map {} instance {}).",
                bot->GetName(), target->GetName(), bot->GetMapId(), bot->GetInstanceId());
        }
        // Ближний — выставляем позицию без клиентского подтверждения.
        else if (bot->IsBeingTeleportedNear())
        {
            bot->SetSemaphoreTeleportNear(0);
            WorldLocation const& dest = bot->GetTeleportDest();
            bot->UpdatePosition(dest, true);
            bot->SetFallInformation(GameTime::GetGameTime().count(), dest.GetPositionZ());
            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} перемещён за {}.",
                bot->GetName(), target->GetName());
        }
    }
}

namespace AiBotsParty
{
    bool IsBotPlayer(Player* player)
    {
        return player && sBotMgr && sBotMgr->IsBot(player->GetGUID().GetCounter());
    }

    bool AddOwnedBots(Player* owner)
    {
        if (!owner)
            return false;

        Group* group = owner->GetGroup();
        if (!group)
        {
            group = new Group();
            if (!group->Create(owner))
            {
                delete group;
                return false;
            }
            sGroupMgr->AddGroup(group);
        }

        uint32 added = 0;
        for (auto const& data : sBotMgr->GetBotsOfOwner(owner->GetGUID().GetCounter()))
        {
            Player* bot = FindBotPlayer(data.guid);
            if (!bot || !bot->IsInWorld() || !bot->IsAlive())
                continue;
            if (bot->GetGroup() == group)
                continue;
            // Чужую группу (например, LFG) не ломаем.
            if (bot->GetGroup())
                continue;

            if (group->AddMember(bot, 0))
                ++added;
        }

        if (added)
            group->BroadcastGroupUpdate();

        if (added)
            LOG_INFO("server.loading", ">> mod-ai-bots: в отряд игрока {} добавлено ботов: {}.",
                owner->GetName(), added);

        return added > 0;
    }

    bool RemoveOwnedBots(Player* owner)
    {
        if (!owner)
            return false;

        Group* group = owner->GetGroup();
        if (!group)
            return false;

        bool removed = false;
        for (auto const& data : sBotMgr->GetBotsOfOwner(owner->GetGUID().GetCounter()))
        {
            Player* bot = FindBotPlayer(data.guid);
            if (!bot || bot->GetGroup() != group)
                continue;

            group->RemoveMember(bot->GetGUID());
            removed = true;
        }

        if (removed)
            group->BroadcastGroupUpdate();

        return removed;
    }

    uint32 CountOwnedBotsInGroup(Player* owner)
    {
        if (!owner)
            return 0;

        Group* group = owner->GetGroup();
        if (!group)
            return 0;

        uint32 count = 0;
        for (auto const& data : sBotMgr->GetBotsOfOwner(owner->GetGUID().GetCounter()))
        {
            Player* bot = FindBotPlayer(data.guid);
            if (bot && bot->GetGroup() == group)
                ++count;
        }

        return count;
    }

    void OnPlayerMapChanged(Player* player)
    {
        if (!player || IsBotPlayer(player))
            return;

        if (!player->GetGroup())
            return;

        ObjectGuid::LowType guid = player->GetGUID().GetCounter();
        if (std::find(s_pendingOwners.begin(), s_pendingOwners.end(), guid) == s_pendingOwners.end())
            s_pendingOwners.push_back(guid);
    }

    void Update(uint32 /*diff*/)
    {
        if (s_pendingOwners.empty())
            return;

        std::vector<ObjectGuid::LowType> pending;
        pending.swap(s_pendingOwners);

        for (ObjectGuid::LowType guid : pending)
        {
            Player* owner = FindBotPlayer(guid);
            if (!owner || !owner->IsInWorld())
                continue;

            Group* group = owner->GetGroup();
            if (!group)
                continue;

            for (auto const& slot : group->GetMemberSlots())
            {
                Player* member = ObjectAccessor::FindConnectedPlayer(slot.guid);
                if (!member || member == owner || !IsBotPlayer(member))
                    continue;

                TeleportBotTo(member, owner);
            }
        }
    }
}
