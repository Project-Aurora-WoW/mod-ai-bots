/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotGear.h
 *
 * Экипировка ботов: наборы брони по типам (cloth/leather/mail/plate)
 * и оружию по классам. Выдаётся при входе бота в мир.
 */

#ifndef AI_BOTS_BOT_GEAR_H
#define AI_BOTS_BOT_GEAR_H

#include "Define.h"
#include "Define.h"
#include "SharedDefines.h"

#include <cstdint>
#include <string>
#include <vector>

class Player;

namespace AiBotsGear
{
    // itemEntry  по слотам: голова, плечи, грудь, ноги, ступни, руки,
    // пояс, запястья, оружие(правая рука), лук/дальний бой.
    struct BotGearSet
    {
        uint32 head, shoulders, chest, legs, feet, hands, waist, wrists;
        uint32 mainHand;
        uint32 ranged;
    };

    BotGearSet const& GetGearForClass(uint8 cls);

    // Случайная внешность: skin, face, hairStyle, hairColor, facialFeature, tattoos.
    void RandomizeAppearance(uint8& skin, uint8& face, uint8& hairStyle,
                             uint8& hairColor, uint8& facialFeature, uint8& tattoos);

    // Надевает набор экипировки на бота (слоты, которые ещё пусты).
    void EquipGearFor(Player* bot);
}

#endif // AI_BOTS_BOT_GEAR_H
