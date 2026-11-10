CREATE TABLE IF NOT EXISTS `mod_ai_bots_stats` (
  `botGuid` int unsigned NOT NULL,
  `pvpKills` int unsigned NOT NULL DEFAULT 0,
  `pveKills` int unsigned NOT NULL DEFAULT 0,
  `deaths` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`botGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Статистика PvP/PvE AI-ботов';
