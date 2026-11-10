/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotTerritory.h
 *
 * Территории: точки спавна «принадлежат» фракциям и периодически
 * переходят из рук в руки с объявлением в мир.
 */

#ifndef AI_BOTS_BOT_TERRITORY_H
#define AI_BOTS_BOT_TERRITORY_H

#include "Define.h"
#include "SharedDefines.h"

#include <cstdint>
#include <string>
#include <vector>

struct BotTerritoryZone
{
    uint32 id;
    std::string name;
    TeamId owner;
};

namespace AiBotsTerritory
{
    // Загрузка территорий и запуск смены владельцев.
    void Init();

    // Периодическое обновление (смена владельца раз в N минут).
    void Update(uint32 diff);

    // Все территории (для отображения в командах).
    std::vector<BotTerritoryZone> const& GetAll();

    // Владелец зоны по имени.
    TeamId GetOwner(std::string const& name);
}

#endif // AI_BOTS_BOT_TERRITORY_H
