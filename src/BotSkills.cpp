/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotSkills.cpp
 *
 * Боты учат классовые способности по своему уровню (SkillLineAbility.dbc)
 * и одну основную профессию с рецептами; раз в две минуты «работают»
 * профессией — создают предмет, который затем продаётся торговцу.
 */

#include "BotSkills.h"
#include "BotLoot.h"
#include "BotSpells.h"

#include "DBCStores.h"
#include "Log.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <algorithm>
#include <map>
#include <vector>

namespace
{
    // Основные профессии WotLK.
    std::vector<uint32> const ProfessionSkillLines =
    {
        171, // Alchemy
        164, // Blacksmithing
        333, // Enchanting
        202, // Engineering
        182, // Herbalism
        773, // Inscription
        755, // Jewelcrafting
        165, // Leatherworking
        186, // Mining
        393, // Skinning
        197  // Tailoring
    };

    bool IsProfessionSkillLine(uint32 skillLine)
    {
        return std::find(ProfessionSkillLines.begin(), ProfessionSkillLines.end(), skillLine) != ProfessionSkillLines.end();
    }

    // Известный боту рецепт профессии, создающий предмет.
    uint32 FindKnownCraftRecipe(Player* bot, uint32 skillLine, uint16 skillValue)
    {
        for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
        {
            SkillLineAbilityEntry const* entry = sSkillLineAbilityStore.LookupEntry(i);
            if (!entry || entry->SkillLine != skillLine)
                continue;
            if (entry->MinSkillLineRank > skillValue)
                continue;
            if (!bot->HasSpell(entry->Spell))
                continue;

            SpellInfo const* info = sSpellMgr->GetSpellInfo(entry->Spell);
            if (!info)
                continue;

            for (uint8 eff = 0; eff < MAX_SPELL_EFFECTS; ++eff)
            {
                SpellEffectInfo const& effect = info->Effects[eff];
                if (effect.Effect == SPELL_EFFECT_CREATE_ITEM && effect.MiscValue > 0)
                    return uint32(effect.MiscValue);
            }
        }

        return 0;
    }

    // Выучить заклинание и все его ранги, доступные по уровню.
    void LearnSpellChain(Player* bot, uint32 spellId, uint32& learned)
    {
        for (uint32 id = sSpellMgr->GetFirstSpellInChain(spellId); id; id = sSpellMgr->GetNextSpellInChain(id))
        {
            SpellInfo const* info = sSpellMgr->GetSpellInfo(id);
            if (!info || info->SpellLevel > bot->GetLevel())
                continue;
            if (bot->HasSpell(id))
                continue;

            bot->learnSpell(id, false);
            ++learned;
        }
    }
}

namespace AiBotsSkills
{
    void LearnForLevel(Player* bot)
    {
        if (!bot)
            return;

        uint32 learned = 0;

        for (uint32 spellId : AiBotsSpells::GetSpellsForClass(bot->getClass()))
            LearnSpellChain(bot, spellId, learned);
        for (uint32 spellId : AiBotsSpells::GetHealSpellsForClass(bot->getClass()))
            LearnSpellChain(bot, spellId, learned);
        for (uint32 spellId : AiBotsSpells::GetBuffSpellsForClass(bot->getClass()))
            LearnSpellChain(bot, spellId, learned);

        LOG_INFO("server.loading", ">> mod-ai-bots: бот {} (уровень {}) выучил способностей: {}.",
            bot->GetName(), uint32(bot->GetLevel()), learned);
    }

    void LearnProfessions(Player* bot)
    {
        if (!bot || ProfessionSkillLines.empty())
            return;

        uint32 skillLine = ProfessionSkillLines[bot->GetGUID().GetCounter() % ProfessionSkillLines.size()];
        uint32 maxSkill = std::min<uint32>(uint32(bot->GetLevel()) * 5, 450);
        if (!maxSkill)
            maxSkill = 1;

        bot->SetSkill(uint16(skillLine), 1, uint16(maxSkill), uint16(450));

        uint32 recipes = 0;
        for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
        {
            if (recipes >= 40)
                break;

            SkillLineAbilityEntry const* entry = sSkillLineAbilityStore.LookupEntry(i);
            if (!entry || entry->SkillLine != skillLine)
                continue;
            if (entry->MinSkillLineRank > maxSkill)
                continue;
            if (bot->HasSpell(entry->Spell))
                continue;
            if (!sSpellMgr->GetSpellInfo(entry->Spell))
                continue;

            bot->learnSpell(entry->Spell, false);
            ++recipes;
        }

        LOG_INFO("server.loading", ">> mod-ai-bots: бот {} освоил профессию {} (навык {}), рецептов: {}.",
            bot->GetName(), skillLine, maxSkill, recipes);
    }

    void UpdateProfessions(Player* bot, uint32 diff)
    {
        if (!bot || !bot->IsAlive() || !bot->IsInWorld())
            return;

        static std::map<ObjectGuid::LowType, uint32> s_timers;
        uint32& timer = s_timers[bot->GetGUID().GetCounter()];
        timer += diff;
        if (timer < 120000)
            return;
        timer = 0;

        for (uint32 skillLine : ProfessionSkillLines)
        {
            if (!bot->HasSkill(skillLine))
                continue;

            uint16 skillValue = bot->GetSkillValue(skillLine);
            uint32 itemId = FindKnownCraftRecipe(bot, skillLine, skillValue);
            if (!itemId)
                continue;

            AiBotsLoot::AddCraftedItem(bot, itemId);

            if (skillValue < 450 && uint32(skillValue) < uint32(bot->GetLevel()) * 5)
                bot->SetSkill(uint16(skillLine), 1, uint16(skillValue + 1), uint16(450));

            LOG_INFO("server.loading", ">> mod-ai-bots: бот {} использовал профессию {}: предмет {}.",
                bot->GetName(), skillLine, itemId);
            return;
        }
    }
}
