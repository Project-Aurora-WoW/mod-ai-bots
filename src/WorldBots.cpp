/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/WorldBots.cpp
 *
 * Автономные боты: спавн (SQL / рядом с игроками / стартовые зоны),
 * бой с мобами и враждебными ботами, спеллы, чат, респавн.
 */

#include "WorldBots.h"
#include "BotAI.h"
#include "BotMgr.h"
#include "BotGear.h"
#include "BotGuild.h"
#include "BotVendors.h"
#include "Config_Bots.h"

#include "Chat.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "MapMgr.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldSession.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <unordered_set>

namespace
{
    // Стартовые/столичные зоны для режима SpawnMode = 2.
    std::vector<WorldBotSpawnPoint> const DefaultPoints =
    {
        { 0, -8913.0f,  133.0f, 87.0f,  3.7f }, // Elwynn Forest (Northshire)
        { 0, -9432.0f,  66.0f,  57.0f,  3.7f }, // Elwynn Forest
        { 0, -6240.0f,  331.0f, 89.0f,  4.9f }, // Stormwind outskirts
        { 1, -618.0f,  -4251.0f, 38.0f, 1.5f }, // Durotar (Valley of Trials)
        { 1,   225.0f, -4726.0f, 47.0f, 4.5f }, // Durotar south
    };

    std::vector<uint8> const BotRaces =
    {
        RACE_HUMAN, RACE_ORC, RACE_DWARF, RACE_NIGHTELF, RACE_UNDEAD_PLAYER,
        RACE_TAUREN, RACE_GNOME, RACE_TROLL, RACE_BLOODELF, RACE_DRAENEI
    };

    std::vector<uint8> const BotClasses =
    {
        CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE, CLASS_PRIEST,
        CLASS_SHAMAN, CLASS_MAGE, CLASS_WARLOCK, CLASS_DRUID
    };

    // Случайный уровень автономного бота: диапазон из конфига + бонус элиты.
    uint8 RollWorldBotLevel()
    {
        uint32 minLevel = std::max<uint32>(1, AiBotsConfig::GetWorldBotLevelMin());
        uint32 maxLevel = std::min<uint32>(80, std::max<uint32>(minLevel, AiBotsConfig::GetWorldBotLevelMax()));

        uint32 level = urand(minLevel, maxLevel);
        if (roll_chance_i(AiBotsConfig::GetEliteChance()))
            level = std::min<uint32>(level + 10, 80);

        return static_cast<uint8>(level);
    }

    // Фракция по расе: точки спавна должны быть в своей зоне.
    TeamId TeamForRace(uint8 race)
    {
        switch (race)
        {
            case RACE_HUMAN:
            case RACE_DWARF:
            case RACE_NIGHTELF:
            case RACE_GNOME:
            case RACE_DRAENEI:
                return TEAM_ALLIANCE;
            default:
                return TEAM_HORDE;
        }
    }

    std::string const WorldBotSessionName = "AI-WORLDBOT";

    // Аккаунт автономных ботов — тот же, что в BotMgr (из конфига).
    uint32 const WorldBotAccount = 0; // заглушка; реальный id берётся через AiBotsConfig::GetBotAccountId()
}

WorldBots* WorldBots::instance()
{
    static WorldBots instance;
    return &instance;
}

namespace
{
    // Выравнивание точки спавна по земле: телепорт-точки из game_tele
    // иногда выше или ниже рельефа — боты висели в воздухе или проваливались.
    void SnapSpawnToGround(WorldBotSpawnPoint& spawn)
    {
        if (spawn.category == 0) // города: точки внутри построек, рельеф не показателен
            return;

        if (Map* map = sMapMgr->CreateBaseMap(spawn.mapId))
        {
            float ground = map->GetHeight(spawn.x, spawn.y, spawn.z, true);
            if (ground > -100000.0f)
                spawn.z = ground + 0.5f;
        }
    }

    // Выбираем точку подальше от уже живых ботов: так боты расходятся
    // по разным локациям, а не сбиваются в одну.
    WorldBotSpawnPoint PickSpread(std::vector<WorldBotSpawnPoint> const& fit)
    {
        if (fit.empty())
            return WorldBotSpawnPoint{};
        if (fit.size() == 1)
            return fit.front();

        std::vector<Player*> online;
        sWorldBots->CollectOnlineBots(online);
        if (online.empty())
        {
            uint32 const idx = urand(0, uint32(fit.size()) - 1);
            return fit[idx];
        }

        WorldBotSpawnPoint best = fit[urand(0, fit.size() - 1)];
        float bestDist = -1.0f;
        uint32 const tries = uint32(std::min<size_t>(10, fit.size()));

        for (uint32 i = 0; i < tries; ++i)
        {
            WorldBotSpawnPoint const& cand = fit[urand(0, fit.size() - 1)];
            float nearest = 1.0e9f;
            for (Player* p : online)
            {
                if (!p || p->GetMapId() != cand.mapId)
                    continue;

                float dx = p->GetPositionX() - cand.x;
                float dy = p->GetPositionY() - cand.y;
                float dz = p->GetPositionZ() - cand.z;
                nearest = std::min(nearest, std::sqrt(dx * dx + dy * dy + dz * dz));
            }

            if (nearest > bestDist)
            {
                bestDist = nearest;
                best = cand;
            }
        }

        return best;
    }
}

void WorldBots::SpawnAll()
{
    if (!AiBotsConfig::IsWorldBotsEnabled())
        return;

    _bots.clear();

    uint32 const baseLimit = AiBotsConfig::GetWorldBotsCount();
    uint32 const endgameLimit = AiBotsConfig::IsEndgameBotsEnabled() ? AiBotsConfig::GetEndgameBotsCount() : 0;
    uint32 baseReused = 0, endgameReused = 0;

    // Сначала переиспользуем уже созданных автономных ботов: каждый рестарт
    // не должен плодить десятки новых персонажей в characters.
    std::unordered_set<uint32> owned;
    if (QueryResult owners = WorldDatabase.Query("SELECT `botGuid` FROM `mod_ai_bots_owners`"))
        do { owned.insert(owners->Fetch()[0].Get<uint32>()); } while (owners->NextRow());

    std::string accountList;
    if (QueryResult accounts = LoginDatabase.Query("SELECT `id` FROM `account` WHERE `username` LIKE 'AIBOT%'"))
        do
        {
            if (!accountList.empty())
                accountList += ',';
            accountList += std::to_string(accounts->Fetch()[0].Get<uint32>());
        } while (accounts->NextRow());

    uint32 poolSize = 0;
    if (!accountList.empty())
        if (QueryResult pool = CharacterDatabase.Query("SELECT COUNT(*) FROM `characters` WHERE `account` IN ({})", accountList))
            poolSize = pool->Fetch()[0].Get<uint32>();

    // Доля свежих профилей: часть слотов занимают новые персонажи
    // (другая раса/класс/внешность), остальные — переиспользованные.
    // Пул ограничен двумя комплектами, чтобы characters не рос бесконечно.
    uint32 const slots = baseLimit + endgameLimit;
    uint32 newBase = 0, newEndgame = 0;
    if (slots)
    {
        uint32 const percent = std::min<uint32>(100, AiBotsConfig::GetWorldBotsProfilesNewPercent());
        uint32 const newQuota = (poolSize < slots * 2) ? std::min<uint32>(slots, (slots * percent) / 100) : 0;

        if (newQuota)
        {
            newBase = (newQuota * baseLimit) / slots;
            newEndgame = newQuota - newBase;

            if (newEndgame > endgameLimit)
            {
                newEndgame = endgameLimit;
                newBase = std::min<uint32>(baseLimit, newQuota - newEndgame);
            }
            if (newBase > baseLimit)
            {
                newBase = baseLimit;
                newEndgame = std::min<uint32>(endgameLimit, newQuota - newBase);
            }
        }
    }

    uint32 const reuseBase = baseLimit - newBase;
    uint32 const reuseEndgame = endgameLimit - newEndgame;

    if (!accountList.empty())
    {
        // Случайный порядок: каждый рестарт в мир попадают разные боты,
        // а не одни и те же самые старые персонажи.
        QueryResult chars = CharacterDatabase.Query(
            "SELECT `guid`, `race`, `class`, `level` FROM `characters` WHERE `account` IN ({}) ORDER BY RAND()", accountList);
        if (chars)
        {
            do
            {
                Field* fields = chars->Fetch();
                uint32 guid = fields[0].Get<uint32>();
                uint8 race = fields[1].Get<uint8>();
                uint8 cls = fields[2].Get<uint8>();
                uint8 charLevel = fields[3].Get<uint8>();

                if (owned.count(guid))
                    continue;
                // Торговцы стоят у прилавка — не отправляем их в случайный пул.
                if (AiBotsVendors::IsVendor(guid))
                    continue;
                if (!sObjectMgr->GetPlayerInfo(race, cls))
                    continue;

                bool endgame = false;
                if (endgameLimit && endgameReused < reuseEndgame && charLevel >= 70)
                {
                    endgame = true;
                    ++endgameReused;
                }
                else if (baseReused < reuseBase)
                    ++baseReused;
                else
                    continue;

                // Переносим старого бота в актуальную точку спавна.
                WorldBotSpawnPoint spawn = PickSpawnPoint(TeamForRace(race), endgame);
                SnapSpawnToGround(spawn);

                uint32 zoneId = 0;
                if (Map* map = sMapMgr->CreateBaseMap(spawn.mapId))
                    zoneId = map->GetZoneId(1, spawn.x, spawn.y, spawn.z);
                if (zoneId > 65535) // characters.zone — smallint unsigned
                    zoneId = 0;

                uint8 newLevel = endgame
                    ? uint8(urand(AiBotsConfig::GetEndgameLevelMin(), AiBotsConfig::GetEndgameLevelMax()))
                    : RollWorldBotLevel();

                CharacterDatabase.Execute(
                    "UPDATE `characters` SET `level`={}, `map`={}, `position_x`={}, `position_y`={}, `position_z`={}, `orientation`={}, `zone`={} WHERE `guid`={}",
                    newLevel, spawn.mapId, spawn.x, spawn.y, spawn.z, spawn.o, zoneId, guid);

                _bots.push_back({ guid, false, false, false, 0, 0 });
            } while (chars->NextRow() && (baseReused < reuseBase || endgameReused < reuseEndgame));
        }
    }

    for (uint32 i = baseReused; i < baseLimit; ++i)
        SpawnOne(i, false);
    for (uint32 i = endgameReused; i < endgameLimit; ++i)
        SpawnOne(i, true);

    LOG_INFO("server.loading", ">> mod-ai-bots: спавн {} автономных ботов (база {} + эндгейм {}, режим {}): переиспользовано {}, новых профилей {}.",
        slots, baseLimit, endgameLimit, AiBotsConfig::GetWorldBotsSpawnMode(),
        baseReused + endgameReused, uint32(_bots.size()) - (baseReused + endgameReused));

    // Перемешиваем очередь: боты выходят вперемешку из разных локаций,
    // а не строго по списку (иначе Нордскол появляется последним).
    std::shuffle(_bots.begin(), _bots.end(), std::mt19937{ std::random_device{}() });

    // Постепенный выход в мир: сразу — только первые StartupCount,
    // остальные появляются по таймеру, чтобы мир наполнялся на глазах.
    uint32 const startup = AiBotsConfig::GetWorldBotsSpawnStartupCount();
    for (size_t i = 0; i < _bots.size(); ++i)
        _bots[i].admitted = (i < startup);

    if (AiBotsConfig::GetWorldBotsSpawnIntervalSec())
        LOG_INFO("server.loading", ">> mod-ai-bots: постепенный спавн — сразу {}, далее по {} с (всего {}).",
            std::min<size_t>(startup, _bots.size()), AiBotsConfig::GetWorldBotsSpawnIntervalSec(), _bots.size());
}

bool WorldBots::SpawnOne(uint32 index, bool endgame)
{
    // Каждому автономному боту — свой класс (по кругу) и случайная
    // совместимая раса: комбинация обязана существовать в playercreateinfo,
    // иначе Player::LoadFromDB не создаст игрока (бот не войдёт в мир).
    uint8 cls = BotClasses[index % BotClasses.size()];

    std::vector<uint8> races;
    for (uint8 r : BotRaces)
        if (sObjectMgr->GetPlayerInfo(r, cls))
            races.push_back(r);

    uint8 race = races.empty() ? uint8(RACE_HUMAN) : races[urand(0, races.size() - 1)];
    if (!sObjectMgr->GetPlayerInfo(race, cls))
    {
        race = RACE_HUMAN;
        cls = CLASS_WARRIOR;
    }

    std::string name = sBotMgr->GenerateUniqueBotName();
    ObjectGuid::LowType lowGuid = sObjectMgr->GetGenerator<HighGuid::Player>().Generate();

    // Уровень: обычные боты — из диапазона конфига, эндгейм — 70-80.
    uint8 level = endgame
        ? uint8(urand(AiBotsConfig::GetEndgameLevelMin(), AiBotsConfig::GetEndgameLevelMax()))
        : RollWorldBotLevel();

    // Случайная внешность: у каждого автономного бота свой облик.
    uint8 skin, face, hairStyle, hairColor, facialFeature, tattoos;
    AiBotsGear::RandomizeAppearance(skin, face, hairStyle, hairColor, facialFeature, tattoos);

    // Точка появления: без координат персонаж остаётся в 0,0,0 и «висит» над океаном.
    WorldBotSpawnPoint spawn = PickSpawnPoint(TeamForRace(race), endgame);
    SnapSpawnToGround(spawn);

    // Синхронный INSERT: персонаж обязан существовать в БД до первого входа в мир.
    // (Update инициирует вход только при первом тике — к этому моменту запись уже видна.)
    // Выполняем строковым запросом: CHAR_INS_CHARACTER в ядре зарегистрирован как
    // CONNECTION_ASYNC и не подготовлен на синхронном соединении DirectExecute.
    // Зона нужна гильдии и /who: zone=0 даёт «broken zone-data» при загрузке.
    uint32 zoneId = 0;
    if (Map* map = sMapMgr->CreateBaseMap(spawn.mapId))
        zoneId = map->GetZoneId(1, spawn.x, spawn.y, spawn.z);
    if (zoneId > 65535) // characters.zone — smallint unsigned
        zoneId = 0;

    CharacterDatabase.DirectExecute(
        "INSERT INTO characters (guid, account, name, race, class, gender, level, xp, money, "
        "skin, face, hairStyle, hairColor, facialStyle, bankSlots, restState, playerFlags, "
        "taximask, innTriggerId, map, position_x, position_y, position_z, orientation, zone) "
        "VALUES ({}, {}, '{}', {}, {}, {}, {}, 0, 0, {}, {}, {}, {}, 0, 0, 0, 0, '', 0, {}, {}, {}, {}, {}, {})",
        lowGuid, sBotMgr->PrepareBotAccount(lowGuid), name, race, cls, urand(0, 1), level,
        skin, face, hairStyle, hairColor, facialFeature, tattoos,
        spawn.mapId, spawn.x, spawn.y, spawn.z, spawn.o, zoneId);

    // Проверяем, что персонаж реально создан: при сбое INSERT в очередь
    // не должен попадать «призрак», который никогда не войдёт в мир.
    if (!CharacterDatabase.Query("SELECT 1 FROM characters WHERE guid = {}", lowGuid))
    {
        LOG_ERROR("server.loading", ">> mod-ai-bots: не удалось создать персонажа бота (guid {}), пропускаю.", lowGuid);
        return false;
    }

    _bots.push_back({ lowGuid, false, false, false, 0, 0 });
    return true;
}

WorldBotSpawnPoint WorldBots::PickSpawnPoint(TeamId team, bool endgame) const
{
    uint8 mode = AiBotsConfig::GetWorldBotsSpawnMode();

    // Смешанный спавн в спорной зоне Ashenvale: боты обеих фракций
    // появляются рядом и сразу вступают в PvP друг с другом.
    if (!endgame && AiBotsConfig::IsWorldBotsPvPEnabled() && urand(1, 100) <= AiBotsConfig::GetWorldBotsPvPShare())
    {
        WorldBotSpawnPoint shared;
        shared.mapId = 1;
        shared.x = 3331.0f + (float(int32(urand(0, 800))) - 400.0f) / 100.0f;
        shared.y = 397.0f + (float(int32(urand(0, 800))) - 400.0f) / 100.0f;
        shared.z = 4.0f;
        shared.o = 0.0f;
        return shared;
    }

    // Режим 0: SQL-таблица mod_ai_bots_locations.
    if (mode == 0)
    {
        if (!_sqlPointsLoaded)
        {
            auto* self = const_cast<WorldBots*>(this);
            self->_sqlPoints.clear();
            if (QueryResult result = WorldDatabase.Query("SELECT `mapId`, `x`, `y`, `z`, `o`, `team`, `category` FROM `mod_ai_bots_locations`"))
            {
                uint32 skipped = 0;
                do
                {
                    Field* fields = result->Fetch();

                    WorldBotSpawnPoint point
                    {
                        fields[0].Get<uint32>(),
                        fields[1].Get<float>(),
                        fields[2].Get<float>(),
                        fields[3].Get<float>(),
                        fields[4].Get<float>(),
                        fields[5].Get<uint8>(),
                        fields[6].Get<uint8>()
                    };

                    // Точка вне зонных полигонов даёт zone = 0xFFFFFFFF при
                    // сохранении персонажа (MySQL: out of range для smallint).
                    uint32 zoneId = 0;
                    if (Map* map = sMapMgr->CreateBaseMap(point.mapId))
                        zoneId = map->GetZoneId(1, point.x, point.y, point.z);
                    if (!zoneId || zoneId > 65535)
                    {
                        ++skipped;
                        continue;
                    }

                    self->_sqlPoints.push_back(point);
                } while (result->NextRow());
                LOG_INFO("server.loading", ">> mod-ai-bots: загружено {} точек спавна из mod_ai_bots_locations (пропущено {} без зоны).",
                    self->_sqlPoints.size(), skipped);
            }
            self->_sqlPointsLoaded = true;
        }

        if (!_sqlPoints.empty())
        {
            // Обычные боты — по фракции среди городов/деревень/зон фарма,
            // эндгейм-боты — только в точках категории 3 (Нордскол, Wintergrasp).
            uint8 const botTeam = (team == TEAM_ALLIANCE) ? 1 : 2;
            std::vector<WorldBotSpawnPoint> fit;
            for (WorldBotSpawnPoint const& p : _sqlPoints)
            {
                if (endgame)
                {
                    if (p.category == 3)
                        fit.push_back(p);
                }
                else if (p.category != 3 && (p.team == 0 || p.team == botTeam))
                    fit.push_back(p);
            }

            if (fit.empty())
                fit = _sqlPoints;

            return PickSpread(fit);
        }
    }

    // Режим 1: рядом со случайным онлайн-игроком.
    if (mode == 1)
    {
        std::vector<Player*> online;
        for (auto const& pair : ObjectAccessor::GetPlayers())
            if (Player* p = pair.second)
                if (p->IsInWorld())
                    online.push_back(p);

        if (!online.empty())
        {
            Player* p = online[urand(0, online.size() - 1)];
            float angle = frand(0.0f, 2.0f * float(M_PI));
            float dist = frand(10.0f, 25.0f);
            return { p->GetMapId(), p->GetPositionX() + dist * std::cos(angle),
                     p->GetPositionY() + dist * std::sin(angle), p->GetPositionZ(), 0.0f };
        }
        // Нет игроков онлайн — падаем в режим 2.
    }

    // Режим 2 (или fallback): стартовая зона своей фракции.
    uint32 wantMap = (team == TEAM_ALLIANCE) ? 0 : 1;
    std::vector<WorldBotSpawnPoint> fit;
    for (WorldBotSpawnPoint const& p : DefaultPoints)
        if (team == TEAM_NEUTRAL || p.mapId == wantMap)
            fit.push_back(p);

    if (fit.empty())
        fit = DefaultPoints;

    return fit[urand(0, fit.size() - 1)];
}

void WorldBots::CollectOnlineBots(std::vector<Player*>& out) const
{
    for (WorldBot const& bot : _bots)
        if (Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(bot.guid)))
            out.push_back(player);
}

std::vector<ObjectGuid::LowType> WorldBots::GetBotGuids() const
{
    std::vector<ObjectGuid::LowType> guids;
    guids.reserve(_bots.size());

    for (WorldBot const& bot : _bots)
        guids.push_back(bot.guid);

    return guids;
}

void WorldBots::Update(uint32 diff)
{
    if (!AiBotsConfig::IsWorldBotsEnabled() || _bots.empty())
        return;

    _updateTimer += diff;
    if (_updateTimer < AiBotsConfig::GetUpdateIntervalMs())
        return;
    _updateTimer = 0;

    // Постепенный выход в мир: один бот за интервал.
    uint32 const spawnIntervalSec = AiBotsConfig::GetWorldBotsSpawnIntervalSec();
    if (spawnIntervalSec)
    {
        _releaseTimer += AiBotsConfig::GetUpdateIntervalMs();
        if (_releaseTimer >= spawnIntervalSec * 1000)
        {
            _releaseTimer = 0;
            for (WorldBot& bot : _bots)
            {
                if (bot.admitted)
                    continue;

                bot.admitted = true;
                LOG_INFO("server.loading", ">> mod-ai-bots: постепенный спавн — в мир выходит бот {}.", bot.guid);
                break;
            }
        }
    }

    uint8 const levelCap = static_cast<uint8>(std::min<uint32>(80, AiBotsConfig::GetWorldBotLevelMax()));

    for (WorldBot& bot : _bots)
    {
        if (!bot.admitted)
            continue;

        Player* player = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(bot.guid));

        // Бот ещё не в мире — инициируем вход через сессию без сокета.
        if (!player)
        {
            if (bot.enterTried)
                continue;

            // Повторяем попытку входа, пока сессия не создастся:
            // падение (например, аккаунт ещё не готов) не должно «замораживать» бота.
            bot.enterTried = sBotMgr->EnterBotWorld(bot.guid);
            continue;
        }


        // Прогрессия: авто-прокачка по таймеру (если включена в конфиге).
        uint32 const progressionCap = std::min<uint32>(levelCap, AiBotsConfig::GetProgressionMaxLevel());
        if (AiBotsConfig::IsProgressionEnabled() && AiBotsConfig::IsProgressionAutoLevelEnabled()
            && player->IsAlive() && player->GetLevel() < progressionCap)
        {
            uint32 const intervalMs = AiBotsConfig::GetProgressionAutoLevelIntervalSec() * 1000;
            bot.xpTimerMs += AiBotsConfig::GetUpdateIntervalMs();

            if (bot.xpTimerMs >= intervalMs)
            {
                bot.xpTimerMs = 0;
                player->GiveLevel(player->GetLevel() + 1);
                AiBotsGuild::AwardLevelXp(player);
            }
        }
        else if (bot.xpTimerMs)
            bot.xpTimerMs = 0;

        // Вступление в гильдию ботов.
        AiBotsGuild::AddMember(player);

        if (!player->IsAlive())
        {
            if (bot.alive)
            {
                // Только что погиб: запускаем таймер респавна.
                bot.alive = false;
                bot.respawnTimerMs = AiBotsConfig::GetWorldBotsRespawnSeconds() * 1000;
                LOG_DEBUG("server.loading", ">> mod-ai-bots: автономный бот {} погиб, респавн через {} с.",
                    bot.guid, AiBotsConfig::GetWorldBotsRespawnSeconds());
            }
            continue;
        }

        if (!bot.alive)
        {
            // Бот снова жив (респавн выполнен).
            bot.alive = true;
            bot.respawnTimerMs = 0;
        }
    }
}
