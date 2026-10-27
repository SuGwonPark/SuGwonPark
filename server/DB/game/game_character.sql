
DROP TABLE IF EXISTS `character`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `character` (
  `playerID` bigint unsigned NOT NULL AUTO_INCREMENT,
  `userUID` bigint unsigned NOT NULL,
  `name` varchar(50) NOT NULL,
  `createTime` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `deleteTime` datetime DEFAULT NULL,
  PRIMARY KEY (`playerID`),
  UNIQUE KEY `name_UNIQUE` (`name`),
  KEY `userUID_idx` (`userUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

