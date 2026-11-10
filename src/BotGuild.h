/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotGuild.h
 *
 * Гильдии ботов — отдельные для каждой фракции (Альянс и Орда),
 * у каждой свой XP, уровень, казна и лидер. Прокачка как в Cataclysm:
 * уровни 1–25 и гильдейские перки. Сохранение в mod_ai_bots_guild
 * с колонкой faction (0 = Альянс, 1 = Орда).
 */

#ifndef AI_BOTS_BOT_GUILD_H
#define AI_BOTS_BOT_GUILD_H

#include "Define.h"
#include "SharedDefines.h"

#include <cstdint>
#include <string>

class Player;
class Unit;

namespace AiBotsGuild
{
    // Восстановление уровней обеих гильдий из БД (вызывается при старте).
    void Init();

    // Периодическое обновление: вступление ботов в гильдию своей фракции.
    void Update(uint32 diff);

    // Бот вступает в гильдию своей фракции.
    void AddMember(Player* bot);

    // Начисление XP гильдии фракции бота за убийство.
    void AwardKillXp(Player* bot, Unit* victim);

    // Начисление XP за достижение ботом нового уровня.
    void AwardLevelXp(Player* bot);

    // --- Гильдейские перки (бонусы членам гильдии бота) ---

    // Множитель персонального XP бота, % (100 = без бонуса).
    uint32 GetXpBonusPct();

    // Множитель золота бота, % (100 = без бонуса).
    uint32 GetGoldBonusPct();

    // Скорость духа: +% (0 = нет).
    uint32 GetMoveSpeedBonusPct();

    // Пополнение казны гильдии фракции бота.
    void DepositToVault(uint32 copper);

    // --- Данные гильдий (в этой реализации гильдия единая) ---

    uint32 GetLevel();
    uint32 GetXp();
    uint32 GetXpForNextLevel();
    uint32 GetVault();
    uint32 GetMemberCount();
    std::string const& GetGuildNameFor(TeamId team);
}

#endif // AI_BOTS_BOT_GUILD_H
