-- mod-ai-bots: точки спавна по всем локациям Азерота и Нордскола.
-- Источник: game_tele (внешние карты 0/1/571), одна точка на сетку 100 ярдов.
-- Курируемые точки (города/деревни) остаются, автоматические помечаются 'auto:'.

DELETE FROM `mod_ai_bots_locations` WHERE `comment` LIKE 'auto:%';

INSERT INTO `mod_ai_bots_locations` (`mapId`, `x`, `y`, `z`, `o`, `team`, `category`, `comment`)
SELECT t.`map`, t.`position_x`, t.`position_y`, t.`position_z`, t.`orientation`, 0,
       IF(t.`map` = 571, 3, 2), CONCAT('auto:', LEFT(t.`name`, 55))
FROM `game_tele` t
JOIN (
    SELECT `map`, FLOOR(`position_x` / 100) AS `gx`, FLOOR(`position_y` / 100) AS `gy`, MIN(`id`) AS `keepId`
    FROM `game_tele`
    WHERE `map` IN (0, 1, 571)
    GROUP BY `map`, `gx`, `gy`
) k ON k.`keepId` = t.`id`
WHERE t.`name` NOT LIKE '%Shop%'
  AND t.`name` NOT LIKE '%Store%'
  AND t.`name` NOT LIKE '%Inn%'
  AND t.`name` NOT LIKE '%Bank%'
  AND t.`name` NOT LIKE '%Auction%'
  AND t.`name` NOT LIKE '%GM%'
  AND t.`name` NOT LIKE '%Test%';
