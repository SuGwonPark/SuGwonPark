DROP TABLE IF EXISTS `item`;
CREATE TABLE `item` (
  `itemUID` bigint unsigned NOT NULL,          
  `playerID` bigint unsigned NOT NULL,
  `itemID` int unsigned NOT NULL,
  `count` int unsigned NOT NULL DEFAULT '1',
  `slot` smallint unsigned NOT NULL,
  `createTime` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`itemUID`),
  KEY `IDX_playerID` (`playerID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;