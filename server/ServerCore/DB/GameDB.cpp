#include "pch.h"
#include "GameDB.h"

void GameDB::SaveCharacterProgressAsync(
    uint64_t playerID,
    uint64_t gold,
    uint64_t exp,
    float x,
    float y,
    float z,
    int hp,
    int mp,
    uint32_t zoneID,
    std::function<void(bool)> onDone)
{
    GameDB::Instance().PushTask(
        playerID, [playerID, gold, exp, x, y, z, hp, mp, zoneID, onDone = std::move(onDone)](mysqlx::Session& db) {
            bool ok = false;
            try
            {
                db.sql(
                      "UPDATE `character_state` SET gold = ?, exp = ?,  posX = ? , posY = ? , posZ = ? , hp = ?, mp "
                      "= ?, zoneID = ?, updateTime = NOW() WHERE playerID = ? ")
                    .bind(gold, exp, x, y, z, hp, mp, zoneID, playerID)
                    .execute();
                ok = true;
            }
            catch (const mysqlx::Error& e)
            {
                std::cerr << "[SaveCharacterProgress] " << e.what() << std::endl;
                if (onDone)
                    onDone(ok);
                throw;
            }
            if (onDone)
                onDone(ok);
        });
}

void GameDB::SaveItemAsync(
    uint64_t playerID,
    uint64_t itemUID,
    uint32_t itemID,
    uint32_t count,
    uint16_t slot,
    const std::vector<ItemOptionInfo>& options)
{
    GameDB::PushTask(playerID, [playerID, itemUID, itemID, count, slot, options](mysqlx::Session& db) {
        try
        {
            db.startTransaction();
            db.sql(
                  "INSERT INTO `item` (itemUID, playerID, itemID, `count`, slot) VALUES (?, ?, ?, ?, ?) AS n "
                  "ON DUPLICATE KEY UPDATE `count` = n.`count`, slot = n.slot ")
                .bind(itemUID, playerID, itemID, count, slot)
                .execute();

            if (!options.empty())
            {
                //(?,?,?), (?,?,?) 옵션 한번에 넣기위해 쿼리문 작성
                std::string q = "INSERT IGNORE INTO `item_option` (itemUID, idx, statType, valueType, `value`) VALUES ";
                for (size_t i = 0; i < options.size(); ++i)
                    q += (i == 0) ? "(?, ?, ?, ?, ?)" : ", (?, ?, ?, ?, ?)";

                mysqlx::SqlStatement stmt = db.sql(q);
                for (size_t i = 0; i < options.size(); ++i)
                {
                    stmt.bind(
                        itemUID, static_cast<uint32_t>(i), static_cast<uint32_t>(options[i].statType),
                        static_cast<uint32_t>(options[i].valueType), options[i].value);
                }

                // 옵션 데이터를 한번에 담아 DB에 저장
                stmt.execute();
            }

            db.commit();
        }
        catch (const mysqlx::Error& e)
        {
            try
            {
                db.rollback();
            }
            catch (...)
            {
            }
            std::cerr << "[SaveItem] playerID " << playerID << ", itemUID " << itemUID << ": " << e.what() << std::endl;
            throw;
        }
    });
}

void GameDB::DeleteItemAsync(uint64_t playerID, uint64_t itemUID)
{
    GameDB::PushTask(playerID, [playerID, itemUID](mysqlx::Session& db) {
        try
        {
            db.startTransaction();
            db.sql("DELETE FROM `item_option` WHERE itemUID = ?").bind(itemUID).execute();
            db.sql("DELETE FROM `item` WHERE itemUID = ? AND playerID = ?").bind(itemUID, playerID).execute();
            db.commit();
        }
        catch (const mysqlx::Error& e)
        {
            try
            {
                db.rollback();
            }
            catch (...)
            {
            }
            std::cerr << "[DeleteItem] playerID " << playerID << ", itemUID " << itemUID << ": " << e.what()
                      << std::endl;
            throw;
        }
    });
}
