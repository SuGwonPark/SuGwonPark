
DROP TABLE IF EXISTS `item_option`;
CREATE TABLE `item_option` (
  `itemUID` bigint unsigned NOT NULL,
  `idx` tinyint unsigned NOT NULL,
  `statType` tinyint unsigned NOT NULL,
  `valueType` tinyint unsigned NOT NULL,
  `value` int NOT NULL,
  PRIMARY KEY (`itemUID`, `idx`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;