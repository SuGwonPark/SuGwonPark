
DROP TABLE IF EXISTS `item_option`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `item_option` (
  `itemUID` bigint unsigned NOT NULL,
  `itemID` int unsigned NOT NULL,
  `enhance` tinyint NOT NULL DEFAULT '0',
  `options` json DEFAULT NULL,
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`itemUID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;
