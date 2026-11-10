/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotNameGen.cpp
 *
 * Слоговый конструктор имён: сочетания слогов дают тысячи уникальных
 * комбинаций, звучащих как «лорные» фэнтезийные имена (Thalgrim, Sylvara...).
 */

#include "BotNameGen.h"

#include "Log.h"
#include "Util.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace
{
    // Начальные слоги.
    std::vector<std::string> const Prefixes =
    {
        "Thal", "Gor", "Mal", "Syl", "Bre", "Kae", "Dor", "Vel", "Zur", "Fen",
        "Ael", "Mor", "Kor", "Bral", "Ner", "Gal", "Ur", "Vash", "Lyr", "Dra",
        "El", "Tor", "Grim", "Ash", "Bel", "Cael", "Drak", "Fyr", "Hald", "Ith"
    };

    // Средние части (могут пропускаться).
    std::vector<std::string> const Middles =
    {
        "a", "o", "i", "e", "aeth", "ara", "ori", "in", "un", "eth",
        "an", "il", "um", "yr", "esh", "ul", "arn", "ith", "orn", "ella"
    };

    // Окончания.
    std::vector<std::string> const Suffixes =
    {
        "grim", "wyn", "thak", "ria", "dric", "mash", "vira", "gorn", "liel", "shan",
        "dor", "wen", "mir", "thas", "gan", "rion", "nor", "vain", "zul", "kath",
        "rok", "elle", "thorn", "dun", "wynn", "rek", "shade", "maw", "fang", "gar"
    };

    // Максимальная длина имени персонажа в движке.
    uint32 const MaxNameLength = 12;
}

std::string AiBotsNameGen::Generate()
{
    std::string name;

    // До 10 попыток уложиться в лимит длины.
    for (int attempt = 0; attempt < 10; ++attempt)
    {
        name = Prefixes[urand(0, Prefixes.size() - 1)];

        // Средняя часть — с вероятностью ~60%.
        if (roll_chance_f(60.0f))
            name += Middles[urand(0, Middles.size() - 1)];

        name += Suffixes[urand(0, Suffixes.size() - 1)];

        if (name.length() <= MaxNameLength)
            break;

        name.clear();
    }

    if (name.empty())
    {
        // Крайний случай: короткая комбинация гарантированно влезает.
        name = Prefixes[urand(0, Prefixes.size() - 1)] + Suffixes[urand(0, Suffixes.size() - 1)];
    }

    // Первая буква заглавная, остальные строчные (требование движка).
    if (!name.empty())
    {
        name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        std::transform(name.begin() + 1, name.end(), name.begin() + 1,
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }

    return name;
}
