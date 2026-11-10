/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotVendors.h
 *
 * Боты-торговцы: игроки могут смотреть список товаров (.bot shop)
 * и покупать предметы (.bot buy <номер>) за игровое золото.
 */

#ifndef AI_BOTS_BOT_VENDORS_H
#define AI_BOTS_BOT_VENDORS_H

#include "Define.h"
#include "ObjectGuid.h"

#include <cstdint>
#include <string>
#include <vector>

class Player;

struct BotVendorItem
{
    uint32 itemId;
    uint32 price;      // в медяках
    uint32 stock;      // 0 = бесконечно
};

namespace AiBotsVendors
{
    // Спавн ботов-торговцев при старте мира.
    void SpawnAll();

    // Периодическое обновление.
    void Update(uint32 diff);

    // Список товаров торговца (nullptr, если бот не торговец).
    std::vector<BotVendorItem> const* GetStock(ObjectGuid::LowType botGuid);

    // Покупка предмета игроком у торговца. Возвращает текст ошибки или пустую строку.
    std::string Buy(Player* buyer, ObjectGuid::LowType vendorGuid, uint32 itemIndex);

    // Является ли персонаж ботом-торговцем.
    bool IsVendor(ObjectGuid::LowType botGuid);

    // Ближайший торговец в радиусе (nullptr — рядом нет).
    Player* FindNearest(Player* seeker, float range);
}

#endif // AI_BOTS_BOT_VENDORS_H
