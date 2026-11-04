CREATE TABLE `item` (
  `itemUID` bigint NOT NULL AUTO_INCREMENT,
  `playerID` bigint NOT NULL,
  `itemID` int NOT NULL,
  `count` int NOT NULL DEFAULT '1',
  `slot` smallint NOT NULL DEFAULT '0',
  `createTime` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`itemUID`),
  KEY `IDX_playerID_itemID` (`playerID`,`itemID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci