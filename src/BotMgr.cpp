/*
 * mod-ai-bots — модуль AI-ботов для AzerothCore
 * Файл: src/BotMgr.cpp
 *
 * Реализация менеджера ботов.
 *
 * Подход: бот — это реальный персонаж в базе characters, который входит
 * в мир через собственный WorldSession (LoginQueryHolder), как это делают
 * игроки. Поведением управляет BotAI (см. BotAI.h/cpp).
 */

#include "BotMgr.h"
#include "Config_Bots.h"
#include "BotAI.h"
#include "BotNameGen.h"
#include "BotGear.h"
#include "BotSkills.h"

#include "CharacterCache.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldSession.h"

#include "AccountMgr.h"
#include "SRP6.h"

#include <chrono>
#include <memory>
#include <thread>

#include <algorithm>
#include <array>
#include <random>

namespace
{
    std::string const BotSessionName = "AI-BOT";
    uint32 const BotAccountFallback = 0; // внутренняя заглушка, если AiBots.AccountId = 0

    // Аккаунт ботов: из конфига или заглушка.
    uint32 GetBotAccount()
    {
        uint32 id = AiBotsConfig::GetBotAccountId();
        return id ? id : BotAccountFallback;
    }

    // Уникальный аккаунт для конкретного бота (режим UniqueAccount.Enable).
    // Создаёт аккаунт при первом обращении, далее возвращает его id.
    uint32 EnsureUniqueBotAccount(ObjectGuid::LowType botGuid)
    {
        std::string name = AiBotsConfig::GetUniqueAccountPrefix() + std::to_string(botGuid);
        uint32 id = sAccountMgr->GetId(name);
        if (id)
            return id;

        // AccountMgr::CreateAccount выполняет INSERT асинхронно — GetId сразу после него
        // может не найти запись. Создаём аккаунт СИНХРОННО (DirectExecute),
        // затем читаем id через синхронный Query.
        auto [salt, verifier] = Acore::Crypto::SRP6::MakeRegistrationData(name, "no-password");
        auto toHex = [](auto const& arr) {
            static constexpr char digits[] = "0123456789abcdef";
            std::string out;
            out.reserve(arr.size() * 2);
            for (uint8 b : arr)
            {
                out.push_back(digits[b >> 4]);
                out.push_back(digits[b & 0x0F]);
            }
            return out;
        };
        std::string saltHex = toHex(salt);
        std::string verifierHex = toHex(verifier);
        std::string sqlName = name; // AIBOT<guid> — латиница и цифры, но экранируем для надёжности
        LoginDatabase.EscapeString(sqlName);
        LoginDatabase.DirectExecute(
            "INSERT INTO account (username, salt, verifier, expansion, reg_mail, email, joindate) "
            "VALUES ('{}', UNHEX('{}'), UNHEX('{}'), {}, '', '', NOW())",
            sqlName, saltHex, verifierHex,
            uint8(sWorld->getIntConfig(CONFIG_EXPANSION)));

        id = sAccountMgr->GetId(name);
        if (!id)
        {
            LOG_ERROR("server.loading", ">> mod-ai-bots: не удалось создать аккаунт '{}' (guid {}).", name, botGuid);
            return 0;
        }

        LOG_INFO("server.loading", ">> mod-ai-bots: создан аккаунт '{}' (id {}) для бота guid {}.", name, id, botGuid);
        return id;
    }

    // Аккаунт для входа бота: уникальный (если включено) или общий из конфига.
    uint32 ResolveBotAccount(ObjectGuid::LowType botGuid)
    {
        if (AiBotsConfig::IsUniqueAccountEnabled())
            return EnsureUniqueBotAccount(botGuid);
        return GetBotAccount();
    }

    bool IsValidRaceClass(uint8 race, uint8 cls)
    {
        // Авторитетная проверка: комбинация должна существовать в playercreateinfo,
        // иначе Player::LoadFromDB откажется загружать персонажа.
        return sObjectMgr->GetPlayerInfo(race, cls) != nullptr;
    }

    // Фракция по расе: 1 — Альянс, 2 — Орда.
    uint8 GetRaceFaction(uint8 race)
    {
        switch (race)
        {
            case RACE_HUMAN:
            case RACE_DWARF:
            case RACE_NIGHTELF:
            case RACE_GNOME:
            case RACE_DRAENEI:
                return 1;
            default:
                return 2;
        }
    }

    // Случайная допустимая пара раса/класс в рамках фракции владельца.
    void RandomRaceClass(uint8 ownerRace, uint8& race, uint8& cls)
    {
        static constexpr std::array<uint8, 5> allianceRaces = { RACE_HUMAN, RACE_DWARF, RACE_NIGHTELF, RACE_GNOME, RACE_DRAENEI };
        static constexpr std::array<uint8, 5> hordeRaces = { RACE_ORC, RACE_UNDEAD_PLAYER, RACE_TAUREN, RACE_TROLL, RACE_BLOODELF };
        auto const& races = (GetRaceFaction(ownerRace) == 1) ? allianceRaces : hordeRaces;

        for (int attempt = 0; attempt < 64; ++attempt)
        {
            uint8 r = races[urand(0, static_cast<uint32>(races.size()) - 1)];
            uint8 c = static_cast<uint8>(urand(CLASS_WARRIOR, CLASS_DRUID));

            // Рыцарь смерти начинается с 55 уровня — случайным ботам он не подходит.
            if (c == CLASS_DEATH_KNIGHT)
                continue;

            if (IsValidRaceClass(r, c))
            {
                race = r;
                cls = c;
                return;
            }
        }

        race = ownerRace ? ownerRace : RACE_HUMAN;
        cls = CLASS_WARRIOR;
    }

    void ParseRaceClass(std::string const& value, uint8& race, uint8& cls)
    {
        race = RACE_HUMAN;
        cls = CLASS_WARRIOR;
        auto pos = value.find(':');
        if (pos == std::string::npos)
            return;

        try
        {
            int r = std::stoi(value.substr(0, pos));
            int c = std::stoi(value.substr(pos + 1));
            if (r > 0 && c > 0)
            {
                race = static_cast<uint8>(r);
                cls = static_cast<uint8>(c);
            }
        }
        catch (std::exception const&)
        {
            // Оставляем значения по умолчанию.
        }
    }
}

BotMgr* BotMgr::instance()
{
    static BotMgr instance;
    return &instance;
}

std::string BotMgr::GenerateUniqueBotName()
{
    // Ленивая загрузка имён из таблицы mod_ai_bots_names (world DB).
    if (!_botNamesLoaded)
    {
        LoadBotNames();
        _botNamesLoaded = true;
    }

    // Fallback, если таблица пуста или БД недоступна.
    if (_botNames.empty())
    {
        static std::vector<std::string> const fallback =
        {
            "BotWarrior", "BotMage", "BotHunter", "BotRogue", "BotPriest",
            "BotPaladin", "BotShaman", "BotWarlock", "BotDruid",
            "Aibo", "Bit", "Botnik", "Botar", "Botwen"
        };
        _botNames = fallback;
    }

    uint8 namesMode = AiBotsConfig::GetNamesMode();

    // Режим 1: чистый генератор — до 25 попыток найти незанятое имя.
    if (namesMode == 1)
    {
        for (int attempt = 0; attempt < 25; ++attempt)
        {
            std::string candidate = AiBotsNameGen::Generate();
            if (!IsNameTaken(candidate))
                return candidate;
        }

        // Крайний случай: генератор с числовым суффиксом.
        std::string base = AiBotsNameGen::Generate();
        for (uint32 suffix = 2; suffix < 100; ++suffix)
        {
            std::string candidate = base.substr(0, 12 - std::to_string(suffix).length()) + std::to_string(suffix);
            if (!IsNameTaken(candidate))
                return candidate;
        }

        return base;
    }

    // Режимы 0 и 2: таблица имён.
    std::vector<std::string> candidates = _botNames;
    std::shuffle(candidates.begin(), candidates.end(), std::mt19937(std::random_device()()));

    for (std::string const& candidate : candidates)
    {
        if (!IsNameTaken(candidate))
            return candidate;
    }

    // Таблица исчерпана.
    if (namesMode == 2)
    {
        // Смешанный режим: переходим на генератор.
        for (int attempt = 0; attempt < 25; ++attempt)
        {
            std::string candidate = AiBotsNameGen::Generate();
            if (!IsNameTaken(candidate))
                return candidate;
        }
    }

    // Режим 0 (или генератор не помог): числовой суффикс.
    if (candidates.empty())
        return AiBotsNameGen::Generate();

    std::string base = candidates[urand(0, candidates.size() - 1)];
    for (uint32 suffix = 2; suffix < 100; ++suffix)
    {
        // Ограничение имени персонажа — 12 символов.
        std::string candidate = base.substr(0, 12 - std::to_string(suffix).length()) + std::to_string(suffix);
        if (!IsNameTaken(candidate))
            return candidate;
    }

    // Крайне маловероятно: используем guid как суффикс.
    return base.substr(0, 6) + std::to_string(sObjectMgr->GetGenerator<HighGuid::Player>().Generate());
}

bool BotMgr::IsNameTaken(std::string const& name) const
{
    // 1. Имя уже у бота в памяти (создан в текущей сессии).
    for (auto const& pair : _allBots)
        if (pair.second.name == name)
            return true;

    // 2. Имя занято другим персонажем в БД.
    if (sCharacterCache->GetCharacterCacheByName(name))
        return true;

    return false;
}

void BotMgr::LoadBotNames()
{
    _botNames.clear();

    QueryResult result = WorldDatabase.Query("SELECT `name` FROM `mod_ai_bots_names`");
    if (!result)
    {
        LOG_WARN("server.loading", ">> mod-ai-bots: таблица mod_ai_bots_names пуста или отсутствует — используются встроенные имена.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        std::string name = fields[0].Get<std::string>();
        if (!name.empty())
            _botNames.push_back(name);
    } while (result->NextRow());

    LOG_INFO("server.loading", ">> mod-ai-bots: загружено {} имён ботов из mod_ai_bots_names.", _botNames.size());
}

bool BotMgr::CreateBotForPlayer(Player* owner, uint8 race, uint8 cls)
{
    if (!owner || !AiBotsConfig::IsEnabled())
        return false;

    ObjectGuid::LowType ownerGuid = owner->GetGUID().GetCounter();

    if (GetBotCountForPlayer(ownerGuid) >= AiBotsConfig::GetMaxBotsPerPlayer())
    {
        ChatHandler(owner->GetSession()).PSendSysMessage("[AI-Bot] Достигнут лимит ботов ({}).", AiBotsConfig::GetMaxBotsPerPlayer());
        return false;
    }

    // Если раса/класс не заданы — случайные в рамках фракции владельца
    // (или фиксированные из конфига, если случайность отключена).
    if (!race || !cls)
    {
        uint8 cfgRace, cfgCls;
        ParseRaceClass(AiBotsConfig::GetDefaultRaceClass(), cfgRace, cfgCls);

        if (AiBotsConfig::IsRandomRaceClassEnabled())
            RandomRaceClass(owner ? owner->getRace() : cfgRace, race, cls);
        else
        {
            race = cfgRace;
            cls = cfgCls;
        }
    }

    if (!IsValidRaceClass(race, cls))
    {
        ChatHandler(owner->GetSession()).PSendSysMessage("[AI-Bot] Недопустимая комбинация раса/класс ({}:{})", race, cls);
        return false;
    }

    std::string name = GenerateUniqueBotName();

    // Уровень бота: по умолчанию как у владельца, AiBots.SyncOwnerLevel задаёт
    // верхнюю границу (0 = без ограничения).
    uint8 level = 1;
    if (owner)
    {
        uint32 ownerLevel = std::max<uint32>(1, owner->GetLevel());
        if (AiBotsConfig::GetSyncOwnerLevel() > 0)
            ownerLevel = std::min<uint32>(ownerLevel, AiBotsConfig::GetSyncOwnerLevel());

        level = static_cast<uint8>(std::min<uint32>(ownerLevel, MAX_LEVEL));
    }

    // Случайная внешность: у каждого бота свой облик.
    uint8 skin, face, hairStyle, hairColor, facialFeature, tattoos;
    AiBotsGear::RandomizeAppearance(skin, face, hairStyle, hairColor, facialFeature, tattoos);

    ObjectGuid::LowType lowGuid = sObjectMgr->GetGenerator<HighGuid::Player>().Generate();

    // Аккаунт бота: уникальный (каждому — свой) или общий из конфига.
    uint32 accountId = ResolveBotAccount(lowGuid);
    if (AiBotsConfig::IsUniqueAccountEnabled() && !accountId)
    {
        ChatHandler(owner->GetSession()).PSendSysMessage("[AI-Bot] Не удалось создать аккаунт для бота.");
        return false;
    }

    // Создаём запись персонажа в БД (упрощённо, минимальный набор полей).
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHARACTER);
    stmt->SetData(0, lowGuid);
    stmt->SetData(1, accountId);
    stmt->SetData(2, name);
    stmt->SetData(3, race);
    stmt->SetData(4, cls);
    stmt->SetData(5, uint8(GENDER_MALE)); // боты создаются мужского пола по умолчанию
    stmt->SetData(6, level);
    stmt->SetData(7, skin);
    stmt->SetData(8, face);
    stmt->SetData(9, hairStyle);
    stmt->SetData(10, hairColor);
    stmt->SetData(11, facialFeature);
    stmt->SetData(12, tattoos);

    BotData data;
    data.guid = lowGuid;
    data.ownerGuid = ownerGuid;
    data.accountId = accountId;
    data.name = name;
    data.race = race;
    data.cls = cls;
    data.level = level;
    data.inWorld = false;

    _botsByOwner[ownerGuid].push_back(data);
    _allBots[lowGuid] = data;
    _ownersLoaded.insert(ownerGuid);

    ChatHandler(owner->GetSession()).PSendSysMessage("[AI-Bot] Бот '{}' создан (раса {}, класс {}). Ожидание входа в мир...", name, race, cls);

    // Записываем связь бот → владелец в world DB для восстановления после рестарта.
    WorldDatabase.Execute(
        "INSERT INTO `mod_ai_bots_owners` (`botGuid`, `ownerGuid`, `name`, `race`, `class`, `level`) VALUES ({}, {}, '{}', {}, {}, {})",
        lowGuid, ownerGuid, name, race, cls, level);

    // Вход в мир после фактической записи персонажа в БД.
    CharacterDatabase.Execute(stmt);

    // Ставим бота рядом с владельцем: без этого персонаж грузится в 0,0,0.
    if (owner)
        CharacterDatabase.Execute(
            "UPDATE `characters` SET `map`={}, `position_x`={}, `position_y`={}, `position_z`={}, `orientation`={}, `zone`={} WHERE `guid`={}",
            owner->GetMapId(), owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ(),
            owner->GetOrientation(), owner->GetZoneId(), lowGuid);

    ScheduleEnterWorld(lowGuid, ownerGuid);

    return true;
}

void BotMgr::ScheduleEnterWorld(ObjectGuid::LowType botGuid, ObjectGuid::LowType ownerGuid)
{
    _pendingLogins.push_back({ botGuid, ownerGuid, 3000 }); // 3 секунды на запись в БД
}

void BotMgr::HandleBotLogin(ObjectGuid::LowType botGuid)
{
    auto it = _allBots.find(botGuid);
    if (it == _allBots.end())
        return;

    BotData& data = it->second;
    data.inWorld = true;

    Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(botGuid));
    if (bot)
    {
        // Выдаём экипировку по классу (если слоты пусты).
        AiBotsGear::EquipGearFor(bot);

        Player* owner = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(data.ownerGuid));
        if (owner)
        {
            // Страховка: если позиция не успела примениться в БД, ставим бота рядом с владельцем.
            if (bot->GetMapId() == owner->GetMapId() && bot->GetDistance(owner) > 50.0f)
                bot->NearTeleportTo(owner->GetPositionX(), owner->GetPositionY(), owner->GetPositionZ(), owner->GetOrientation());

            ChatHandler(owner->GetSession()).PSendSysMessage("[AI-Bot] Бот '{}' вошёл в мир рядом с вами.", data.name);
        }
    }
}

bool BotMgr::EnterWorld(ObjectGuid::LowType botGuid, ObjectGuid::LowType ownerGuid)
{
    return EnterBotWorld(botGuid);
}

BotMgr::~BotMgr() = default;

uint32 BotMgr::PrepareBotAccount(ObjectGuid::LowType botGuid)
{
    return ResolveBotAccount(botGuid);
}

void BotMgr::SetBotSiegeMode(ObjectGuid::LowType botGuid, bool enabled)
{
    auto it = _botAIs.find(botGuid);
    if (it != _botAIs.end())
        it->second->SetSiegeMode(enabled);
}

bool BotMgr::EnterBotWorld(ObjectGuid::LowType botGuid)
{
    // Уже в мире.
    if (ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(botGuid)))
        return true;

    ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(botGuid);

    // Аккаунт: сначала из реестра модуля, затем из кеша персонажей
    // (автономные боты живут в WorldBots, а не в _allBots).
    uint32 accountId = 0;
    auto it = _allBots.find(botGuid);
    if (it != _allBots.end())
        accountId = it->second.accountId;

    if (!accountId)
        if (CharacterCacheEntry const* cache = sCharacterCache->GetCharacterCacheByGuid(guid))
            accountId = cache->AccountId;

    // Старые персонажи ботов созданы с account = 0 — выдаём им аккаунт
    // и сразу переносим запись в characters, чтобы ядро могло загрузить игрока.
    if (!accountId)
    {
        accountId = ResolveBotAccount(botGuid);
        if (accountId)
        {
            CharacterDatabase.Execute("UPDATE `characters` SET `account` = {} WHERE `guid` = {}", accountId, botGuid);
            sCharacterCache->UpdateCharacterAccountId(guid, accountId);
        }
    }

    if (!accountId)
    {
        LOG_ERROR("server.loading", ">> mod-ai-bots: не удалось определить аккаунт для входа бота (guid {}).", botGuid);
        return false;
    }

    // Создаём сессию без сокета. ВАЖНО: не регистрируем её в WorldSessionMgr —
    // ядро удаляет сессии без сокета (WorldSession::Update -> "if (!m_Socket) return false"),
    // что приводило к мгновенному логауту бота. Ядро (master) не патчим.
    std::string botName = "";
    WorldSession* session = new WorldSession(accountId, std::move(botName), 0, nullptr, SEC_PLAYER, 2, 0, LOCALE_enUS, 0, false, false, 0);

    // Загрузка персонажа штатным пайплайном ядра.
    // LoginQueryHolder сам запросы не выполняет: в обычном потоке их запускает
    // CharacterDatabase.DelayQueryHolder(...), а результат обрабатывает апдейт сессии.
    // У сессии без сокета апдейта нет, поэтому выполняем холдер здесь и ждём готовности.
    LOG_INFO("server.loading", ">> mod-ai-bots: вход бота guid {} (аккаунт {}).", botGuid, accountId);
    std::shared_ptr<LoginQueryHolder> holder = std::make_shared<LoginQueryHolder>(accountId, ObjectGuid::Create<HighGuid::Player>(botGuid));
    if (!holder->Initialize())
    {
        LOG_INFO("server.loading", ">> mod-ai-bots: LoginQueryHolder::Initialize() не удался (guid {}).", botGuid);
        delete session;
        return false;
    }

    SQLQueryHolderCallback callback = CharacterDatabase.DelayQueryHolder(holder);
    callback.AfterComplete([](SQLQueryHolderBase const&) {});
    bool ready = false;
    for (uint32 waited = 0; waited < 10000 && !(ready = callback.InvokeIfReady()); ++waited)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    if (!ready)
    {
        LOG_ERROR("server.loading", ">> mod-ai-bots: запросы входа для guid {} не выполнились за 10 с.", botGuid);
        delete session;
        return false;
    }

    session->HandlePlayerLoginFromDB(*holder);

    Player* botPlayer = session->GetPlayer();
    if (!botPlayer)
    {
        LOG_INFO("server.loading", ">> mod-ai-bots: HandlePlayerLoginFromDB не создал игрока (guid {}).", botGuid);
        delete session;
        return false;
    }

    _botSessions[botGuid] = session;

    // Экипировка: раньше выдавалась только ботам владельцев, автономные
    // оставались голыми. Надеваем всем (заполняются только пустые слоты).
    AiBotsGear::EquipGearFor(botPlayer);

    // Способности и профессия: боты учатся сами, тренер им не нужен.
    AiBotsSkills::LearnForLevel(botPlayer);
    AiBotsSkills::LearnProfessions(botPlayer);

    uint32 equipped = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        if (botPlayer->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
            ++equipped;

    LOG_INFO("server.loading", ">> mod-ai-bots: бот {} экипирован, предметов: {}.", botPlayer->GetName(), equipped);

    // Подключаем поведение: боты владельцев следуют за хозяином,
    // автономные (world bots) — живут сами (патруль, бой, чат).
    auto ai = std::make_unique<BotAI>(botPlayer);
    auto ownerIt = _allBots.find(botGuid);
    if (ownerIt != _allBots.end() && ownerIt->second.ownerGuid)
    {
        ai->SetOwner(ObjectAccessor::FindConnectedPlayer(
            ObjectGuid::Create<HighGuid::Player>(ownerIt->second.ownerGuid)));
        ai->SetAutonomous(false);
    }
    else
    {
        ai->SetAutonomous(true);
    }
    _botAIs[botGuid] = std::move(ai);

    return true;
}

bool BotMgr::RemoveBot(Player* owner, std::string const& botName)
{
    if (!owner)
        return false;

    ObjectGuid::LowType ownerGuid = owner->GetGUID().GetCounter();
    auto& bots = _botsByOwner[ownerGuid];

    for (auto it = bots.begin(); it != bots.end(); ++it)
    {
        if (it->name != botName)
            continue;

        ObjectGuid::LowType botGuid = it->guid;

        // Сначала корректный логаут сессии (иначе characters.online останется = 1).
        LogoutBot(botGuid);

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        // Удаляем запись персонажа (в этом ядре доступна только CHAR_DEL_CHARACTER;
        // остальные связанные таблицы ядро очищает по внешним ключам/само).
        if (auto* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHARACTER))
        {
            stmt->SetData(0, botGuid);
            trans->Append(stmt);
        }

        CharacterDatabase.CommitTransaction(trans);

        // Связь бот → владелец живёт в таблице модуля (world DB) — чистим отдельным запросом.
        WorldDatabase.Execute("DELETE FROM `mod_ai_bots_owners` WHERE `botGuid` = {}", botGuid);

        // Сброс кеша персонажей, чтобы имя сразу стало свободным.
        if (sCharacterCache->GetCharacterCacheByGuid(ObjectGuid::Create<HighGuid::Player>(botGuid)))
            sCharacterCache->DeleteCharacterCacheEntry(ObjectGuid::Create<HighGuid::Player>(botGuid), it->name);

        _allBots.erase(botGuid);
        bots.erase(it);

        ChatHandler(owner->GetSession()).PSendSysMessage("[AI-Bot] Бот '{}' удалён.", botName);
        return true;
    }

    ChatHandler(owner->GetSession()).PSendSysMessage("[AI-Bot] Бот '{}' не найден.", botName);
    return false;
}

void BotMgr::RemoveAllBots(Player* owner)
{
    if (!owner)
        return;

    ObjectGuid::LowType ownerGuid = owner->GetGUID().GetCounter();
    auto bots = _botsByOwner[ownerGuid]; // копия, т.к. RemoveBot меняет контейнер
    for (auto const& data : bots)
        RemoveBot(owner, data.name);
}

void BotMgr::LogoutBot(ObjectGuid::LowType botGuid)
{
    _botAIs.erase(botGuid);

    if (Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(botGuid)))
        if (WorldSession* session = bot->GetSession())
            session->LogoutPlayer(true);

    // Удаляем сессию, которой владеет модуль (в менеджере её нет).
    auto sit = _botSessions.find(botGuid);
    if (sit != _botSessions.end())
    {
        WorldSession* session = sit->second;
        _botSessions.erase(sit);
        delete session; // игрок уже вышел, деструктор повторный логаут не делает
    }

    auto it = _allBots.find(botGuid);
    if (it != _allBots.end())
        it->second.inWorld = false;
}

void BotMgr::Update(uint32 diff)
{
    if (!AiBotsConfig::IsEnabled())
        return;

    uint32 interval = AiBotsConfig::GetUpdateIntervalMs();
    if (!interval)
        return; // защита от деления/логики на нулевом интервале

    _updateTimer += diff;
    if (_updateTimer < interval)
        return;
    _updateTimer = 0;

    // Отложенные входы в мир
    for (auto it = _pendingLogins.begin(); it != _pendingLogins.end();)
    {
        PendingLogin& p = *it;
        p.remainingMs -= interval;

        if (p.remainingMs > interval)
        {
            ++it;
            continue;
        }

        if (EnterWorld(p.botGuid, p.ownerGuid))
            HandleBotLogin(p.botGuid);

        it = _pendingLogins.erase(it);
    }

    // Поведение ботов: движение, бой, чат.
    for (auto aiIt = _botAIs.begin(); aiIt != _botAIs.end(); )
    {
        Player* bot = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(aiIt->first));
        if (!bot)
        {
            aiIt = _botAIs.erase(aiIt);
            continue;
        }

        // Сессии ботов не тикают в WorldSessionMgr: Map::Update зовёт
        // Player::Update с нулевым диффом, и сплайны движения стоят.
        // Тикаем бота сами — тогда работают MovePoint, реген и таймеры.
        bot->Update(interval);

        aiIt->second->UpdateAI(interval);
        ++aiIt;
    }

    // Если владелец вышел из мира — боты тоже выходят.
    for (auto it = _botsByOwner.begin(); it != _botsByOwner.end();)
    {
        ObjectGuid::LowType ownerGuid = it->first;
        bool ownerOnline = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(ownerGuid)) != nullptr;

        if (!ownerOnline)
        {
            for (auto const& data : it->second)
                if (data.inWorld)
                    LogoutBot(data.guid);

            // Чистим также глобальный реестр, чтобы он не рос бесконечно.
            for (auto const& data : it->second)
                _allBots.erase(data.guid);
            _ownersLoaded.erase(ownerGuid);

            it = _botsByOwner.erase(it);
        }
        else
            ++it;
    }
}

Player* BotMgr::GetBotPlayer(ObjectGuid::LowType botGuid) const
{
    auto it = _allBots.find(botGuid);
    if (it == _allBots.end() || !it->second.inWorld)
        return nullptr;
    return ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(botGuid));
}

bool BotMgr::IsBot(ObjectGuid::LowType botGuid) const
{
    return _botSessions.find(botGuid) != _botSessions.end();
}

BotAI* BotMgr::GetBotAI(ObjectGuid::LowType botGuid) const
{
    auto it = _botAIs.find(botGuid);
    return it != _botAIs.end() ? it->second.get() : nullptr;
}

std::vector<BotData> const& BotMgr::GetBotsOfOwner(ObjectGuid::LowType ownerGuid) const
{
    static std::vector<BotData> const empty;
    auto it = _botsByOwner.find(ownerGuid);
    return it != _botsByOwner.end() ? it->second : empty;
}

std::string const& BotMgr::GetBotName(ObjectGuid::LowType botGuid) const
{
    static std::string const empty;
    auto it = _allBots.find(botGuid);
    return it != _allBots.end() ? it->second.name : empty;
}

void BotMgr::HandlePlayerLogout(Player* player)
{
    if (!player)
        return;

    // Выходим боты владельца (данные сохраняются, боты остаются в БД).
    ObjectGuid::LowType ownerGuid = player->GetGUID().GetCounter();
    auto it = _botsByOwner.find(ownerGuid);
    if (it == _botsByOwner.end())
        return;

    for (auto const& data : it->second)
        if (data.inWorld)
            LogoutBot(data.guid);
}

void BotMgr::HandlePlayerLogin(Player* player)
{
    if (!player || !AiBotsConfig::IsEnabled())
        return;

    ObjectGuid::LowType ownerGuid = player->GetGUID().GetCounter();

    // Восстанавливаем ботов из БД (после рестарта сервера связь терялась).
    LoadOwnedBots(ownerGuid);

    if (!AiBotsConfig::IsAutoSpawnEnabled())
        return;

    uint32 count = AiBotsConfig::GetBotsPerLogin();
    for (uint32 i = 0; i < count; ++i)
        CreateBotForPlayer(player);
}

void BotMgr::LoadOwnedBots(ObjectGuid::LowType ownerGuid)
{
    // Владелец онлайн, ботов нет в памяти: грузим привязки из БД (после рестарта
    // сервера связь бот → владелец доступна только там). Повторно не грузим.
    if (_botsByOwner.count(ownerGuid) || _ownersLoaded.count(ownerGuid))
        return;

    QueryResult result = WorldDatabase.Query(
        "SELECT `botGuid`, `name`, `race`, `class`, `level` FROM `mod_ai_bots_owners` WHERE `ownerGuid` = {}", ownerGuid);
    if (!result)
        return;

    _ownersLoaded.insert(ownerGuid);

    do
    {
        Field* fields = result->Fetch();
        BotData data;
        data.guid = fields[0].Get<uint32>();
        data.ownerGuid = ownerGuid;
        data.name = fields[1].Get<std::string>();
        data.race = fields[2].Get<uint8>();
        data.cls = fields[3].Get<uint8>();
        data.level = fields[4].Get<uint8>();
        data.inWorld = false;

        // Пропускаем битые записи (персонаж удалён вручную из characters).
        if (CharacterCacheEntry const* cache = sCharacterCache->GetCharacterCacheByGuid(ObjectGuid::Create<HighGuid::Player>(data.guid)))
        {
            data.accountId = cache->AccountId;

            // Миграция: в режиме уникальных аккаунтов переводим ботов,
            // сидящих на общем аккаунте, на их собственный аккаунт AIBOT<guid>.
            if (AiBotsConfig::IsUniqueAccountEnabled() && data.accountId == GetBotAccount())
            {
                uint32 newAccount = EnsureUniqueBotAccount(data.guid);
                if (newAccount && newAccount != data.accountId)
                {
                    CharacterDatabase.Execute("UPDATE `characters` SET `account` = {} WHERE `guid` = {}", newAccount, data.guid);
                    sCharacterCache->UpdateCharacterAccountId(ObjectGuid::Create<HighGuid::Player>(data.guid), newAccount);
                    data.accountId = newAccount;
                    LOG_INFO("server.loading", ">> mod-ai-bots: бот {} (guid {}) переведён на аккаунт {} (id {}).",
                        data.name, data.guid, AiBotsConfig::GetUniqueAccountPrefix() + std::to_string(data.guid), newAccount);
                }
            }
        }
        else
        {
            WorldDatabase.Execute("DELETE FROM `mod_ai_bots_owners` WHERE `botGuid` = {}", data.guid);
            continue;
        }

        // Уровень: подтягиваем старых ботов к уровню владельца
        // (ранее созданные боты оставались на 1 уровне).
        if (Player* owner = ObjectAccessor::FindConnectedPlayer(ObjectGuid::Create<HighGuid::Player>(ownerGuid)))
        {
            uint32 ownerLevel = std::max<uint32>(1, owner->GetLevel());
            if (AiBotsConfig::GetSyncOwnerLevel() > 0)
                ownerLevel = std::min<uint32>(ownerLevel, AiBotsConfig::GetSyncOwnerLevel());

            uint8 desiredLevel = static_cast<uint8>(std::min<uint32>(ownerLevel, MAX_LEVEL));
            if (data.level < desiredLevel)
            {
                data.level = desiredLevel;
                CharacterDatabase.Execute("UPDATE `characters` SET `level` = {} WHERE `guid` = {}", desiredLevel, data.guid);
                WorldDatabase.Execute("UPDATE `mod_ai_bots_owners` SET `level` = {} WHERE `botGuid` = {}", desiredLevel, data.guid);
            }
        }

        _botsByOwner[ownerGuid].push_back(data);
        _allBots[data.guid] = data;

        // Ставим восстановленного бота на вход в мир (после записи в БД/кэш),
        // иначе после рестарта боты остаются «в базе, но не в игре».
        ScheduleEnterWorld(data.guid, ownerGuid);
    } while (result->NextRow());

    if (!_botsByOwner[ownerGuid].empty())
        LOG_INFO("server.loading", ">> mod-ai-bots: восстановлено {} ботов владельца {}.",
            _botsByOwner[ownerGuid].size(), ownerGuid);
}

uint32 BotMgr::GetBotCountForPlayer(ObjectGuid::LowType ownerGuid) const
{
    auto it = _botsByOwner.find(ownerGuid);
    return it != _botsByOwner.end() ? static_cast<uint32>(it->second.size()) : 0;
}
