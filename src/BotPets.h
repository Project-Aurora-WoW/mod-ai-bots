/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotPets.h
 *
 * Питомцы ботов: охотники призывают зверей, чернокнижники — демонов.
 * Питомцы следуют за ботом и сражаются с его целью.
 */

#ifndef AI_BOTS_BOT_PETS_H
#define AI_BOTS_BOT_PETS_H

#include "Define.h"

#include <cstdint>

class Player;

namespace AiBotsPets
{
    // Вызывается периодически: суммон питомцев ботам-охотникам/чернокнижникам.
    void Update(uint32 diff);

    // Есть ли у класса питомец (hunter / warlock).
    bool ClassHasPet(uint8 cls);

    // Попытка призвать питомца боту.
    bool SummonPetFor(Player* bot);
}

#endif // AI_BOTS_BOT_PETS_H
