/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotSpells.cpp
 */

#include "BotSpells.h"

namespace AiBotsSpells
{
    std::vector<uint32> const& GetSpellsForClass(uint8 cls)
    {
        static std::vector<uint32> const warrior = { 20252, 71, 2457 };     // Intercept, Defensive Stance, Battle Shout
        static std::vector<uint32> const paladin = { 20271, 25780, 21084 }; // Judgement, Seal of Righteousness, Consecration
        static std::vector<uint32> const hunter  = { 56641, 1978, 2643 };   // Steady Shot, Auto Shot, Multi-Shot
        static std::vector<uint32> const rogue   = { 1752, 53, 1766 };      // Sinister Strike, Kick, Cheap Shot
        static std::vector<uint32> const priest  = { 589, 15407, 8092 };    // Shadow Word: Pain, Mind Blast, Mind Blast (low)
        static std::vector<uint32> const shaman  = { 403, 8042, 8050 };     // Lightning Bolt, Earth Shock, Flame Shock
        static std::vector<uint32> const mage    = { 133, 116, 2136 };      // Fireball, Frostbolt, Fire Blast
        static std::vector<uint32> const warlock = { 686, 172, 348 };       // Shadow Bolt, Corruption, Immolate
        static std::vector<uint32> const druid   = { 339, 5176, 8921 };     // Entangling Roots, Wrath, Moonfire

        switch (cls)
        {
            case CLASS_WARRIOR:  return warrior;
            case CLASS_PALADIN:  return paladin;
            case CLASS_HUNTER:   return hunter;
            case CLASS_ROGUE:    return rogue;
            case CLASS_PRIEST:   return priest;
            case CLASS_SHAMAN:   return shaman;
            case CLASS_MAGE:     return mage;
            case CLASS_WARLOCK:  return warlock;
            case CLASS_DRUID:    return druid;
            default:             return warrior;
        }
    }

    std::vector<uint32> const& GetHealSpellsForClass(uint8 cls)
    {
        static std::vector<uint32> const paladin = { 635, 19750, 20473 };   // Holy Light, Flash Heal rank1(=Flash of Light), Holy Shock
        static std::vector<uint32> const priest  = { 2050, 2061, 139 };     // Lesser Heal, Flash Heal, Renew
        static std::vector<uint32> const shaman  = { 331, 8004, 1064 };     // Healing Wave, Healing Wave (low rank), Chain Heal
        static std::vector<uint32> const druid   = { 5185, 18562, 774 };    // Healing Touch, Swiftmend-ish rank, Rejuvenation
        static std::vector<uint32> const empty;

        switch (cls)
        {
            case CLASS_PALADIN: return paladin;
            case CLASS_PRIEST:  return priest;
            case CLASS_SHAMAN:  return shaman;
            case CLASS_DRUID:   return druid;
            default:            return empty;
        }
    }

    std::vector<uint32> const& GetBuffSpellsForClass(uint8 cls)
    {
        static std::vector<uint32> const warrior  = { 2457, 2458 };         // Battle Shout, Commanding Shout
        static std::vector<uint32> const paladin  = { 19740, 25782, 465 };  // Blessing of Might, Greater Blessing of Might, Devotion Aura
        static std::vector<uint32> const mage     = { 1459, 23028 };        // Arcane Intellect, Arcane Brilliance
        static std::vector<uint32> const priest   = { 1243, 10952 };        // Power Word: Fortitude, Divine Spirit
        static std::vector<uint32> const druid    = { 1126, 467 };          // Mark of the Wild, Thorns
        static std::vector<uint32> const empty;

        switch (cls)
        {
            case CLASS_WARRIOR: return warrior;
            case CLASS_PALADIN: return paladin;
            case CLASS_MAGE:    return mage;
            case CLASS_PRIEST:  return priest;
            case CLASS_DRUID:   return druid;
            default:            return empty;
        }
    }

    std::vector<std::string> const& GetBattleCries()
    {
        static std::vector<std::string> const phrases =
        {
            "For the Alliance!",
            "For the Horde!",
            "Attack!",
            "Let's fight!",
            "Bring it on!",
            "You will not pass!"
        };
        return phrases;
    }

    std::vector<std::string> const& GetVictoryPhrases()
    {
        static std::vector<std::string> const phrases =
        {
            "Easy win!",
            "Another one down.",
            "That was a good fight!",
            "Victory is ours!",
            "Is that all you've got?"
        };
        return phrases;
    }

    std::vector<std::string> const& GetIdlePhrases()
    {
        static std::vector<std::string> const phrases =
        {
            "Beautiful day, isn't it?",
            "Anyone needs help?",
            "I'm looking for a group.",
            "These lands are dangerous...",
            "Have you seen any dragons around?",
            "Time for an adventure!"
        };
        return phrases;
    }

    std::vector<std::string> const& GetDeathPhrases()
    {
        static std::vector<std::string> const phrases =
        {
            "I... will return...",
            "Not like this...",
            "Tell my family... I fought well..."
        };
        return phrases;
    }

    std::vector<std::string> const& GetFleePhrases()
    {
        static std::vector<std::string> const phrases =
        {
            "I need to fall back!",
            "This is too much for me!",
            "Retreat! I'm wounded!",
            "I can't take much more of this!"
        };
        return phrases;
    }

    std::vector<std::string> const& GetHealPhrases()
    {
        static std::vector<std::string> const phrases =
        {
            "Healing myself, hold on!",
            "The Light heals my wounds.",
            "Nature restores me.",
            "Mending wounds..."
        };
        return phrases;
    }
}
