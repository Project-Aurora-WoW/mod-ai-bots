-- mod-ai-bots: структура и справочные данные таблиц модуля.
-- Путь: modules/mod-ai-bots/data/sql/db-world/mod_ai_bots_tables.sql
-- Применяется ядром автоматически (AzerothCore ModuleDBUpdater), идемпотентно.

/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `mod_ai_bots_guild` (
  `id` int unsigned NOT NULL,
  `level` int unsigned NOT NULL DEFAULT '1',
  `xp` int unsigned NOT NULL DEFAULT '0',
  `vault` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Уровень гильдии ботов (mod-ai-bots)';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `mod_ai_bots_locations` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `mapId` smallint unsigned NOT NULL DEFAULT '0',
  `x` float NOT NULL,
  `y` float NOT NULL,
  `z` float NOT NULL,
  `o` float NOT NULL DEFAULT '0',
  `team` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0=любая, 1=Альянс, 2=Орда',
  `category` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '0=город, 1=деревня, 2=зона фарма',
  `comment` varchar(64) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=40 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Точки спавна автономных AI-ботов';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `mod_ai_bots_names` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(12) NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_name` (`name`)
) ENGINE=InnoDB AUTO_INCREMENT=50 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Имена для AI-ботов (mod-ai-bots)';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `mod_ai_bots_owners` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `botGuid` int unsigned NOT NULL,
  `ownerGuid` int unsigned NOT NULL,
  `name` varchar(12) NOT NULL,
  `race` tinyint unsigned NOT NULL,
  `class` tinyint unsigned NOT NULL,
  `level` tinyint unsigned NOT NULL DEFAULT '1',
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_botguid` (`botGuid`),
  KEY `idx_ownerguid` (`ownerGuid`)
) ENGINE=InnoDB AUTO_INCREMENT=406 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Владельцы AI-ботов (mod-ai-bots)';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `mod_ai_bots_siege_targets` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `mapId` smallint unsigned NOT NULL DEFAULT '0',
  `x` float NOT NULL,
  `y` float NOT NULL,
  `z` float NOT NULL,
  `name` varchar(64) NOT NULL,
  `faction` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Цели осад городов (mod-ai-bots)';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `mod_ai_bots_territory` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(64) NOT NULL,
  `owner` tinyint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=13 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Территории фракций (mod-ai-bots)';
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE IF NOT EXISTS `mod_ai_bots_vendors` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `botName` varchar(12) NOT NULL,
  `itemId` int unsigned NOT NULL,
  `price` int unsigned NOT NULL DEFAULT '100',
  `stock` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `idx_botname` (`botName`)
) ENGINE=InnoDB AUTO_INCREMENT=5 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Товары ботов-торговцев (mod-ai-bots)';
/*!40101 SET character_set_client = @saved_cs_client */;

INSERT  IGNORE INTO `mod_ai_bots_guild` (`id`, `level`, `xp`, `vault`) VALUES (1,1,0,0);
DELETE FROM `mod_ai_bots_locations`;
INSERT INTO `mod_ai_bots_locations` (`id`, `mapId`, `x`, `y`, `z`, `o`, `team`, `category`, `comment`) VALUES
(1,0,-8913,133,87,3.7,1,1,'Элвинн / Нортшир'),
(2,0,-9432,66,57,3.7,1,1,'Элвиннский лес'),
(3,0,-6240,331,89,4.9,1,0,'Штормград (окраина)'),
(4,1,-618,-4251,38,1.5,2,1,'Дуротар / Долина Испытаний'),
(5,1,225,-4726,47,4.5,2,1,'Дуротар (юг)'),
(6,0,-8908,128,87,3.7,1,1,'Нортширское аббатство'),
(7,1,-616,-4253,38,1.5,2,1,'Долина Испытаний (хаб)'),
(8,0,-8833,629,94,1,1,0,'Штормград'),
(9,0,-4919,-940,502,5,1,0,'Стальгорн'),
(10,1,9950,2284,1341,2,1,0,'Дарнасс'),
(11,1,1630,-4374,32,4,2,0,'Оргриммар'),
(12,1,-1277,125,131,5,2,0,'Громовой Утёс'),
(13,0,1584,240,-52,0,2,0,'Подгород'),
(14,0,-9449,68,56,2,1,1,'Златоземье'),
(15,0,-5597,-483,397,3,1,1,'Каранос'),
(16,0,-9267,-2189,64,2,1,1,'Приозерье'),
(17,0,-10573,-1183,28,0,1,1,'Тёмная Гавань'),
(18,1,6501,482,6,2,1,1,'Аубердин'),
(19,0,-5353,-2949,324,5,1,1,'Телсамар'),
(20,0,-10624,1097,34,1,1,1,'Сторожевой Холм'),
(21,1,327,-4707,15,4,2,1,'Колючий Холм'),
(22,1,-2363,-1914,96,0,2,1,'Лагерь Таурахо'),
(23,0,2259,290,34,1,2,1,'Брилл'),
(24,530,7564,-6872,96,4,2,1,'Тихотень'),
(25,0,-10447,-3262,20,5,2,1,'Каменор'),
(26,0,-10235,1222,44,6,0,2,'Западный край'),
(27,0,-10898,-365,39,3,0,2,'Сумеречный лес'),
(28,1,1928,-2166,94,0,0,2,'Ясеневый лес'),
(29,1,-7931,-3414,81,1,0,2,'Танарис'),
(30,1,6759,-4420,763,4,0,2,'Зимние Ключи'),
(31,1,-4841,1309,81,1,0,2,'Фералас'),
(32,1,-606,2212,93,1,0,2,'Пустоши'),
(33,0,-6779,-3424,242,1,0,2,'Бесплодные земли'),
(34,0,-8119,-1634,133,0,0,2,'Пылающие Степи'),
(35,0,-10345,-2773,22,5,0,2,'Болото Печали'),
(36,0,1729,-1602,63,2,0,2,'Западные Чумные земли'),
(37,0,2301,-4613,74,0,0,2,'Восточные Чумные земли'),
(38,571,4761,2144,423,0,0,3,'Wintergrasp'),
(39,571,5348,2839,409,0,0,3,'Wintergrasp Fortress'),
(40,571,5808,588,661,0,0,3,'Dalaran'),
(41,571,8516,629,547,0,0,3,'Argent Tournament'),
(42,571,7375,1991,622,0,0,3,'Icecrown'),
(43,571,7527,-1261,919,0,0,3,'Storm Peaks');INSERT  IGNORE INTO `mod_ai_bots_names` (`id`, `name`) VALUES (10,'Aibo'),(35,'Ashvane'),(44,'Balgor'),(11,'Bit'),(13,'Botar'),(9,'Botdruid'),(3,'Bothunter'),(2,'Botmage'),(12,'Botnik'),(6,'Botpaladin'),(5,'Botpriest'),(4,'Botrogue'),(7,'Botshaman'),(8,'Botwarlock'),(1,'Botwarrior'),(14,'Botwen'),(23,'Brakthar'),(34,'Bramm'),(45,'Caelwyn'),(19,'Dornhammer'),(38,'Drakhmar'),(22,'Ellowyn'),(28,'Faelira'),(47,'Fenris'),(25,'Grimshaw'),(42,'Grommash'),(37,'Illysia'),(31,'Kaedric'),(17,'Korgath'),(36,'Korvan'),(30,'Liraelle'),(16,'Maevyn'),(48,'Malyna'),(24,'Mirawen'),(32,'Morgraine'),(26,'Naylith'),(43,'Neryssa'),(27,'Ordwin'),(21,'Ranthor'),(33,'Sylthara'),(18,'Sylvara'),(15,'Thalgrim'),(40,'Thalor'),(49,'Urthok'),(20,'Veliska'),(41,'Vexira'),(39,'Wrenna'),(46,'Zarakh'),(29,'Zugthrak');
INSERT  IGNORE INTO `mod_ai_bots_siege_targets` (`id`, `mapId`, `x`, `y`, `z`, `name`, `faction`) VALUES (1,0,-9050,440,94,'Stormwind',0),(2,0,-4981,-881,501,'Ironforge',0),(3,1,1568,-4297,61,'Orgrimmar',1),(4,0,1584,240,58,'Undercity',1);
INSERT  IGNORE INTO `mod_ai_bots_territory` (`id`, `name`, `owner`) VALUES (1,'Elwynn Forest',0),(2,'Dun Morogh',0),(3,'Stormwind',0),(4,'Westfall',0),(5,'Duskwood',0),(6,'Durotar',1),(7,'Mulgore',0),(8,'Orgrimmar',1),(9,'Northern Barrens',1),(10,'Ashenvale',0),(11,'Hillsbrad Foothills',1),(12,'Arathi Highlands',1);
INSERT  IGNORE INTO `mod_ai_bots_vendors` (`id`, `botName`, `itemId`, `price`, `stock`) VALUES (1,'Botwarrior',6948,100,0),(2,'Botmage',118,800,0),(3,'Botmage',858,400,0),(4,'Botpriest',17032,2000,0);

