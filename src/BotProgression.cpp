/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotProgression.cpp
 *
 * Прогрессия ботов: опыт режется множителем из конфига и упирается
 * в кап уровня; при повышении уровня обновляются экипировка и навыки.
 */

#include "BotProgression.h"
#include "BotGear.h"
#include "BotSkills.h"
#include "Config_Bots.h"
#include "Log.h"
#include "Player.h"

void AiBotsProgression::ScaleXp(Player* bot, uint32& amount)
{
    if (!bot || !amount)
        return;

    if (!AiBotsConfig::IsProgressionEnabled())
        return;

    if (bot->GetLevel() >= AiBotsConfig::GetProgressionMaxLevel())
    {
        amount = 0;
        return;
    }

    float const rate = AiBotsConfig::GetProgressionXpRate();
    if (rate <= 0.0f)
    {
        amount = 0;
        return;
    }

    amount = uint32(float(amount) * rate + 0.5f);
    if (!amount)
        amount = 1;
}

void AiBotsProgression::OnLevelChanged(Player* bot, uint8 oldLevel)
{
    if (!bot || !AiBotsConfig::IsProgressionEnabled())
        return;

    if (bot->GetLevel() <= oldLevel)
        return;

    AiBotsGear::EquipGearFor(bot);
    AiBotsSkills::LearnForLevel(bot);

    LOG_INFO("server.loading", ">> mod-ai-bots: бот {} достиг уровня {} (экипировка и навыки обновлены).",
        bot->GetName(), bot->GetLevel());
}
