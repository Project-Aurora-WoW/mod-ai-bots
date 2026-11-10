/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotParty.h
 *
 * Отряд с игроком: боты вступают в группу игрока, следуют за ним
 * и переходят вместе с ним в подземелья.
 */

#ifndef AI_BOTS_BOT_PARTY_H
#define AI_BOTS_BOT_PARTY_H

#include "Define.h"
#include "ObjectGuid.h"

class Player;

namespace AiBotsParty
{
    // Бот — это игрок-сессия без сокета.
    bool IsBotPlayer(Player* player);

    // Добавить всех онлайн-ботов владельца в его группу (создать группу при необходимости).
    bool AddOwnedBots(Player* owner);

    // Вывести ботов владельца из его группы.
    bool RemoveOwnedBots(Player* owner);

    // Сколько ботов владельца состоит в его группе.
    uint32 CountOwnedBotsInGroup(Player* owner);

    // Игрок сменил карту — запомнить, чтобы на следующем тике перевести ботов.
    void OnPlayerMapChanged(Player* player);

    // Тик модуля: перевод ботов к владельцу после смены карты.
    void Update(uint32 diff);
}

#endif // AI_BOTS_BOT_PARTY_H
