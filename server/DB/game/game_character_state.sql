
DROP TABLE IF EXISTS `character_state`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character_state` (
  `playerID` bigint unsigned NOT NULL,
  `class` tinyint unsigned NOT NULL,
  `gold` bigint unsigned NOT NULL DEFAULT '0',
  `exp` bigint NOT NULL DEFAULT '0',
  `hp` int NOT NULL DEFAULT '100',
  `mp` int NOT NULL DEFAULT '50',
  `zoneID` int unsigned NOT NULL DEFAULT '1',
  `posX` float NOT NULL,
  `posY` float NOT NULL,
  `posZ` float NOT NULL,
  `updateTime` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`playerID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_as_cs;
