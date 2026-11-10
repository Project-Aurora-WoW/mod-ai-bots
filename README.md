# mod-ai-bots

[![License: AGPL v3](https://img.shields.io/badge/license-AGPL--3.0-blue.svg)](LICENSE)
[![AzerothCore](https://img.shields.io/badge/AzerothCore-3.3.5a-green.svg)](https://www.azerothcore.org/)

AI-боты для **AzerothCore** (WotLK 3.3.5a): автономные персонажи живут в мире,
качаются, сражаются, берут и сдают квесты, собирают ресурсы, торгуют, объединяются
в группы и гильдии, воюют за территории и осаждают города. Игрок может завести
собственных ботов-спутников и управлять ими командами `.bot`.

Модуль **не требует правок ядра**: боты входят в мир через собственные сессии
и действуют штатным API AzerothCore.

> **Summary (EN).** `mod-ai-bots` adds autonomous AI players to AzerothCore 3.3.5a:
> world bots that level, fight, quest, gather, trade, group up, join guilds, capture
> territories and siege cities, plus player-owned companion bots controlled with
> `.bot` commands. No core patches required. See [Configuration](#конфигурация),
> [Commands](#команды) and [Troubleshooting](#диагностика-и-частые-проблемы).

---

## Содержание

- [Возможности](#возможности)
- [Требования](#требования)
- [Установка](#установка)
- [Быстрый старт](#быстрый-старт)
- [Конфигурация](#конфигурация)
- [Команды](#команды)
- [База данных](#база-данных)
- [Архитектура](#архитектура)
- [Диагностика и частые проблемы](#диагностика-и-частые-проблемы)
- [Обновление](#обновление)
- [Ограничения и планы](#ограничения-и-планы)
- [Участие в разработке](#участие-в-разработке)
- [Лицензия](#лицензия)

## Возможности

### Автономные (мировые) боты

- **Постепенный выход в мир**: несколько ботов сразу, остальные — по таймеру,
  чтобы мир наполнялся на глазах.
- **Спавн по 1300+ точкам** Азерота и Нордскола: города, деревни, дикая местность,
  отдельный пул для карты 571 (Нордскол).
- **Умный выбор точки**: бот выходит в точке, максимально удалённой от уже
  вышедших ботов, — они не собираются в одну кучу.
- **Ограниченный пул**: персонажи переиспользуются между рестартами, доля свежих
  профилей настраивается; `characters` не растёт бесконечно.
- **Эндгейм-боты** 70–80 уровня отдельным пулом для Нордскола, PvP и осад.
- **Возрождение** после смерти по таймеру.

### Поведение

- **Боевой AI**: выбор цели и тактики, лечение союзников, бегство при низком
  здоровье, баффы, применение заклинаний и способностей.
- **Питомцы** охотников и чернокнижников: призыв, лечение, поведение в бою.
- **Квесты**: бот берёт подходящие задания у ближайших квестодателей, идёт к живой
  цели или к точке респавна из `creature`, сдаёт квест эндеру. Недостижимые
  задания отбрасываются, доступные — нет.
- **Сбор ресурсов и профессии**: травничество, горное дело, рыбалка; изучение
  профессий и рецептов.
- **Экипировка по уровню**: броня и оружие подбираются под уровень бота.
- **Группы ботов** и **гильдии** с уровнями и опытом за убийства.
- **PvP**: поля боя, дуэли; статистика PvP/PvE как у обычных игроков.
- **Территории** с переворотом контроля и **осады городов** (Штормград, Оргриммар,
  Стальгорн, Подгород).
- **Боты-торговцы** у банков фракций с настраиваемым ассортиментом.
- **Социальное взаимодействие**: чат, диалоги, шёпот, трейд.
- **Прогрессия**: опыт, множитель опыта, авто-прокачка по таймеру.
- **Корректный UTF-8** в консоли сервера и логах (включая кириллицу).

### Боты игрока

- Создание, удаление и список ботов.
- Спутники следуют за владельцем, вступают в группу, помогают в бою.
- Экипировка, профессии, питомцы, виртуальные кошельки, магазин.

## Требования

- **AzerothCore** (ветка `master`, WotLK 3.3.5a), C++20.
- **MySQL 8.0**.
- **Клиент** 3.3.5a (12340).
- Свободные аккаунты для ботов не нужны: модуль создаёт их сам
  (префикс `AIBOT` + guid персонажа).

## Установка

### 1. Клонирование

```bash
cd azerothcore-wotlk/modules
git clone https://github.com/your-account/mod-ai-bots.git mod-ai-bots
```

### 2. Сборка

Модуль собирается вместе с ядром:

```bash
cd azerothcore-wotlk
cmake -S . -B build \
  -DCMAKE_INSTALL_PREFIX=/path/to/install \
  -DTOOLS=0 \
  -DSCRIPTS=static \
  -DMODULES=static
cmake --build build -j$(nproc)
cmake --install build
```

Windows (MSVC):

```powershell
cmake -S . -B build -A x64 -DTOOLS=0 -DSCRIPTS=static -DMODULES=static
cmake --build build --config RelWithDebInfo --target worldserver
```

### 3. Конфигурация

При сборке ядро копирует `conf/mod_ai_bots.conf.dist` в
`configs/modules/mod_ai_bots.conf`. При необходимости создайте файл вручную:

```bash
cp conf/mod_ai_bots.conf.dist /path/to/install/etc/modules/mod_ai_bots.conf
```

### 4. База данных

SQL модуля лежит в `data/sql/db-world` и `data/sql/db-characters` и применяется
ядром автоматически при старте `worldserver` (ModuleDBUpdater). Применение вручную:

```bash
mysql -uroot -p acore_world < data/sql/db-world/mod_ai_bots_tables.sql
mysql -uroot -p acore_world < data/sql/db-world/2026_11_10_bot_spawn_locations.sql
mysql -uroot -p acore_world < data/sql/db-world/2026_11_10_01_bot_names_extra.sql
mysql -uroot -p acore_characters < data/sql/db-characters/2026_11_10_bot_stats.sql
```

Все файлы идемпотентны: повторное применение не создаёт дублей.

### 5. Запуск и проверка

Запустите `worldserver` и найдите в логе строки модуля:

```
>> mod-ai-bots: UTF-8-вывод — проверка: Привет, мир! «Ёжик», ツ ✓
>> mod-ai-bots: спавн 50 автономных ботов (база 40 + эндгейм 10, режим 0)
>> mod-ai-bots: постепенный спавн — сразу 5, далее по 15 с (всего 50)
>> mod-ai-bots: вход бота guid ... (аккаунт ...)
```

Боты в мире проверяются командой `.bot groups` (нужны права).

## Быстрый старт

1. Включите модуль: `AiBots.Enable = 1` в `mod_ai_bots.conf`.
2. Задайте количество ботов: `AiBots.WorldBots.Count` (по умолчанию 40).
3. Перезапустите сервер — боты начнут выходить в мир.
4. Войдите в игру и создайте собственного бота: `.bot add 1:1` (человек-воин).
5. Управление: `.bot follow`, `.bot party`, `.bot list`, `.bot stats`.

## Конфигурация

Все опции находятся в `configs/modules/mod_ai_bots.conf`; шаблон с комментариями —
[`conf/mod_ai_bots.conf.dist`](conf/mod_ai_bots.conf.dist).

### Общие

| Опция | По умолчанию | Описание |
|---|---|---|
| `AiBots.Enable` | `1` | Включение модуля. `0` — полностью выключить |
| `AiBots.AccountId` | `0` | ID общего аккаунта ботов (`0` — заглушка) |
| `AiBots.UniqueAccount.Enable` | `1` | Отдельный аккаунт каждому боту |
| `AiBots.UniqueAccount.Prefix` | `"AIBOT"` | Префикс имён аккаунтов |
| `AiBots.UniqueAccount.StartId` | `10000` | Резерв ID аккаунтов |
| `AiBots.MaxBotsPerPlayer` | `1` | Максимум ботов на игрока |
| `AiBots.AutoSpawn` | `1` | Создавать бота при входе игрока |
| `AiBots.AutoSpawnCount` | `1` | Сколько ботов создавать автоматически |
| `AiBots.UpdateInterval` | `500` | Период обновления AI, мс |
| `AiBots.EnableCombat` | `1` | Боевое поведение |
| `AiBots.FollowDistance` | `4.0` | Дистанция следования за владельцем |
| `AiBots.CommandsForAll` | `1` | Разрешить команды всем игрокам |
| `AiBots.DefaultRaceClass` | `"1:1"` | Раса:класс по умолчанию |
| `AiBots.RandomRaceClass` | `1` | Случайная раса/класс |
| `AiBots.Names.Mode` | `2` | Источник имён ботов |

### Мировые боты

| Опция | По умолчанию | Описание |
|---|---|---|
| `AiBots.WorldBots.Enable` | `1` | Включение автономных ботов |
| `AiBots.WorldBots.Count` | `40` | Количество базовых ботов |
| `AiBots.WorldBots.SpawnMode` | `0` | Режим выбора точек спавна |
| `AiBots.WorldBots.Spawn.StartupCount` | `5` | Сколько выпустить сразу |
| `AiBots.WorldBots.Spawn.IntervalSeconds` | `15` | Интервал выпуска остальных |
| `AiBots.WorldBots.Profiles.NewPercent` | `20` | Доля новых профилей |
| `AiBots.WorldBots.RespawnSeconds` | `60` | Возрождение после смерти |
| `AiBots.WorldBots.Level` | `5` | Уровень новых ботов |
| `AiBots.WorldBots.LevelMin`/`LevelMax` | `1`/`60` | Диапазон уровней |
| `AiBots.WorldBots.UseSpells` | `1` | Использовать заклинания |
| `AiBots.WorldBots.Chat.Enable` | `1` | Чат ботов |
| `AiBots.WorldBots.Chat.BattleCryChance` | `30` | Шанс боевого клича, % |
| `AiBots.WorldBots.Chat.IdleChance` | `2` | Шанс фразы в простое, % |
| `AiBots.WorldBots.Patrol.Radius` | `25` | Радиус патрулирования |
| `AiBots.WorldBots.Leveling` | `1` | Боты развиваются |

### Прогрессия

| Опция | По умолчанию | Описание |
|---|---|---|
| `AiBots.WorldBots.Progression.Enable` | `1` | Прогрессия опыта |
| `AiBots.WorldBots.Progression.MaxLevel` | `80` | Предел прокачки |
| `AiBots.WorldBots.Progression.XpRate` | `1.0` | Множитель опыта |
| `AiBots.WorldBots.Progression.AutoLevel` | `1` | Авто-прокачка |
| `AiBots.WorldBots.Progression.AutoLevel.IntervalSeconds` | `300` | Интервал авто-прокачки |
| `AiBots.SyncOwnerLevel` | `0` | Подгонять уровень под владельца |

### PvP, эндгейм, группы, дуэли

| Опция | По умолчанию | Описание |
|---|---|---|
| `AiBots.WorldBots.PvP.Enable` | `0` | PvP-боты |
| `AiBots.WorldBots.PvP.SharePercent` | `50` | Доля PvP-ботов |
| `AiBots.WorldBots.Endgame.Enable` | `1` | Пул ботов 70–80 |
| `AiBots.WorldBots.Endgame.Count` | `10` | Размер пула |
| `AiBots.WorldBots.Endgame.LevelMin`/`LevelMax` | `70`/`80` | Уровни эндгейма |
| `AiBots.WorldBots.EliteChance` | `10` | Шанс элитного бота, % |
| `AiBots.WorldBots.Duels.Enable` | `1` | Дуэли ботов |
| `AiBots.WorldBots.Duels.Chance` | `5` | Шанс дуэли, % |
| `AiBots.WorldBots.Groups.Enable` | `1` | Группы ботов |
| `AiBots.WorldBots.Groups.MaxSize` | `5` | Размер группы |

### Гильдии, территории, осады

| Опция | По умолчанию | Описание |
|---|---|---|
| `AiBots.Guild.Enable` | `1` | Гильдии ботов |
| `AiBots.Guild.AllianceName` | `"AI Vanguard"` | Гильдия Альянса |
| `AiBots.Guild.HordeName` | `"AI Legion"` | Гильдия Орды |
| `AiBots.Guild.XpPerKill` | `5` | Опыт гильдии за убийство |
| `AiBots.Guild.XpPerLevelBase` | `1000` | Базовый опыт уровня гильдии |
| `AiBots.Guild.MaxLevel` | `25` | Максимальный уровень гильдии |
| `AiBots.Territory.Enable` | `1` | Контроль территорий |
| `AiBots.Territory.FlipMinutes` | `30` | Период переворота контроля |
| `AiBots.Siege.Enable` | `1` | Осады городов |
| `AiBots.Siege.IntervalMinMinutes`/`IntervalMaxMinutes` | `90`/`240` | Интервал осад |
| `AiBots.Siege.AttackerMin`/`AttackerMax` | `5`/`10` | Атакующие |
| `AiBots.Siege.DurationMinutes` | `15` | Длительность осады |

### Торговцы, питомцы, тактики

| Опция | По умолчанию | Описание |
|---|---|---|
| `AiBots.Vendors.Enable` | `1` | Боты-торговцы |
| `AiBots.Vendors.Count` | `3` | Количество торговцев |
| `AiBots.Pets.Enable` | `1` | Питомцы ботов |
| `AiBots.Pets.SummonIntervalSeconds` | `20` | Интервал призыва |
| `AiBots.Tactics.FleeHealthPct` | `20` | Бегство при здоровье, % |
| `AiBots.Tactics.HealThresholdPct` | `50` | Порог лечения, % |
| `AiBots.Tactics.UseBuffs` | `1` | Использовать баффы |

После изменения конфигурации перезапустите `worldserver` либо выполните
`.reload config` (если поддерживается сборкой).

## Команды

| Команда | Описание |
|---|---|
| `.bot add [раса:класс]` | Создать бота, например `.bot add 1:1` |
| `.bot remove [имя]` | Удалить бота (без имени — всех своих) |
| `.bot list` | Список ваших ботов |
| `.bot info` | Подробности о боте |
| `.bot follow` | Бот следует за вами |
| `.bot party` | Взять бота в группу |
| `.bot stats` | Статистика PvP/PvE ботов |
| `.bot shop` | Товары ботов-торговцев |
| `.bot buy <номер>` | Купить товар из списка `.bot shop` |
| `.bot gold` | Виртуальные кошельки ботов |
| `.bot pet` | Призвать питомца ботов |
| `.bot groups` | Список групп автономных ботов |
| `.bot territory` | Контроль территорий |
| `.bot guild` | Гильдии ботов |
| `.bot siege` | Состояние осад |

Примеры:

```
.bot add 1:1      # человек-воин
.bot add 2:7      # орк-шаман
.bot list
.bot follow
.bot party
```

## База данных

### `world`

**`mod_ai_bots_locations`** — точки спавна ботов.

| Поле | Тип | Описание |
|---|---|---|
| `id` | int unsigned | Первичный ключ |
| `mapId` | smallint unsigned | Карта |
| `x`, `y`, `z`, `o` | float | Координаты и ориентация |
| `team` | tinyint unsigned | `0` — любая, `1` — Альянс, `2` — Орда |
| `category` | tinyint unsigned | `0` — город, `1` — деревня, `2` — зона фарма, `3` — Нордскол |
| `comment` | varchar(64) | Название точки; авто-точки помечены `auto:` |

**`mod_ai_bots_names`** — пул имён (`id`, `name`; `name` уникально).

**`mod_ai_bots_owners`** — боты игроков (`botGuid`, `ownerGuid`, `name`, `race`,
`class`, `level`).

**`mod_ai_bots_guild`** — гильдии ботов (`id`, `level`, `xp`, `vault`).

**`mod_ai_bots_territory`** — контроль территорий (`id`, `name`, `owner`).

**`mod_ai_bots_siege_targets`** — цели осад (`mapId`, `x`, `y`, `z`, `name`, `faction`).

**`mod_ai_bots_vendors`** — ассортимент торговцев (`botName`, `itemId`, `price`, `stock`).

### `characters`

**`mod_ai_bots_stats`** — статистика ботов (`botGuid`, `pvpKills`, `pveKills`, `deaths`).

> Файл `mod_ai_bots_tables.sql` при повторном применении перезаписывает
> `mod_ai_bots_locations` базовым набором. Свои точки добавляйте отдельными
> SQL-файлами с `INSERT`, а не правкой этого файла.

## Архитектура

| Файл | Ответственность |
|---|---|
| `src/mod_ai_bots_loader.cpp` | Точка входа: `Addmod_ai_botsScripts()` |
| `src/AiBotsScripts.cpp` | Скрипты ядра, команды `.bot`, порядок обновления |
| `src/BotMgr.*` | Сессии ботов, вход/выход, аккаунты |
| `src/BotAI.*` | Базовый AI, тактики, боевое поведение |
| `src/WorldBots.*` | Пул автономных ботов, спавн, переиспользование |
| `src/BotQuest.*` | Квесты: взятие, движение к целям, сдача |
| `src/BotGear.*`, `BotSkills.*`, `BotSpells.*`, `BotGather.*` | Экипировка, профессии, заклинания, сбор |
| `src/BotGroups.*`, `BotParty.*`, `BotGuild.*` | Группы, отряд, гильдии |
| `src/BotBattleground.*`, `BotSiege.*`, `BotTerritory.*` | PvP, осады, территории |
| `src/BotVendors.*`, `BotLoot.*`, `BotPets.*`, `BotStats.*` | Торговцы, лут, питомцы, статистика |
| `src/BotChat.*`, `BotChatPhrases.h`, `BotDialogue.*`, `BotInteract.*` | Чат, диалоги, взаимодействие |
| `src/BotPersonality.*`, `BotProgression.*`, `BotNameGen.*` | Характер, прогрессия, имена |
| `src/Config_Bots.*` | Загрузка и проверка конфигурации |
| `src/BotConsoleAppender.h` | UTF-8 вывод в консоль и логи |

**Как это работает**

- **Сессии.** Боты входят в мир через собственные сессии без сокета и тикаются
  `BotMgr::Update`, поэтому для ядра они — обычные персонажи.
- **Движение.** Прямые отрезки (`MovePoint` без pathfinding); цель не
  переставляется, пока бот идёт к ней.
- **Квесты.** Бот берёт подходящий kill-квест у ближайшего квестодателя, идёт к
  живой цели или к точке респавна из `creature`, сдаёт квест эндеру. Квест
  бросается только если его целей нет в мире ни на одной карте.
- **Пул.** Список ботов хранится в памяти; персонажи переиспользуются между
  рестартами, аккаунты создаются автоматически с префиксом `AIBOT`.

## Диагностика и частые проблемы

| Симптом | Причина | Решение |
|---|---|---|
| Ботов нет в мире | `AiBots.Enable = 0` или `AiBots.WorldBots.Enable = 0` | Включить опции, перезапустить сервер |
| `has incorrect race/class pair. Can't be loaded` | В `playercreateinfo` нет пары раса/класс | Проверить полноту базы (`playercreateinfo`), пересоздать бота |
| `Unknown column` / `Duplicate entry` при старте | Структура БД не соответствует ядру либо SQL применён вручную | Привести БД к версии ядра, удалить ручные записи из `updates` |
| Кракозябры в консоли | Кодовая страница консоли | Модуль пишет UTF-8; используйте `chcp 65001` или перенаправление в файл |
| Боты стоят на месте | Конфликт с другим модулем ботов | Не запускать одновременно с `mod-playerbots` |
| Бот не берёт квесты | Нет подходящих квестодателей рядом или уровень не подходит | Проверить лог `скан: гверов ...`, переместить бота в стартовую зону |
| Бот не входит в мир | Занятый аккаунт или неверная пара раса/класс | Проверить `AIBOT*` в `auth.account`, лог `вход бота guid` |

Полезные команды диагностики:

```bash
# боты онлайн
mysql -uroot -p acore_characters -e "SELECT COUNT(*) FROM characters WHERE online=1;"
# точки спавна
mysql -uroot -p acore_world -e "SELECT category, COUNT(*) FROM mod_ai_bots_locations GROUP BY category;"
```

## Обновление

```bash
cd azerothcore-wotlk/modules/mod-ai-bots
git pull
cd ../..
cmake --build build -j$(nproc)
```

Новые SQL-файлы применяются автоматически при следующем старте `worldserver`.
Все они идемпотентны, повторный запуск безопасен.

## Ограничения и планы

- Сценарии, требующие клиента (шёпот, трейд, дуэли, визуальная проверка),
  проверяются вручную.
- Вход ботов на поля боя реализован очередью; в ряде случаев требуется доработка
  подтверждения приглашения.
- Арены (очередь и команды) пока не реализованы.
- Модуль рассчитан на WotLK 3.3.5a и актуальный `master` AzerothCore; на старых
  ревизиях API может отличаться.

## Участие в разработке

1. Форкните репозиторий и создайте ветку: `git checkout -b feature/my-change`.
2. Соберите ядро с модулем и проверьте изменения в игре.
3. Соблюдайте стиль окружающего кода (C++20, 4 пробела, `nullptr`, `auto`).
4. Откройте pull request с описанием изменений и шагами проверки.

Сообщения об ошибках приветствуются: укажите версию ядра, конфигурацию модуля и
фрагмент лога `Server.log` / `Errors.log`.

## Лицензия

Проект распространяется по лицензии **GNU Affero General Public License v3.0**.
Полный текст — в файле [LICENSE](LICENSE).

## Благодарности

- [AzerothCore](https://www.azerothcore.org/) — ядро и API.
- Сообществу AzerothCore за документацию и примеры модулей.
