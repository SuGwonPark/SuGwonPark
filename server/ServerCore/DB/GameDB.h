#pragma once
#include "ServerCore/DB/DBThreadPool.h"
#include "ServerCore/Protocol/ProtocolTypes.h"

class GameDB {
public:
	static GameDB& Instance() {
		static GameDB instance;
		return instance;
	}

	// DB 연결
	bool Init(size_t threadCount, const std::string& connUrl) {
		pool_ = std::make_unique<DB::DBThreadPool>(threadCount, connUrl);
		return true;
	}

	void PushTask(DB::DBTask task) {
		if (pool_) {
			pool_->PushTask(std::move(task));
		}
	}

	void PushTask(uint64_t key, DB::DBTask task) {
		if (pool_) {
			pool_->PushTask(key, std::move(task));
		}
	}


	// 큐에 남은 작업을 전부 실행하고 워커 스레드를 종료한다 (서버 종료 시 1회)
	void Shutdown() { pool_.reset(); }

	// 캐릭터 기본 정보 DB 저장
	void SaveCharacterProgressAsync(uint64_t playerID, uint64_t gold, uint64_t exp,  float x, float y, float z, int hp, int mp, uint32_t zoneID, std::function<void(bool)> onDone = nullptr);

	// 인벤토리 1칸 저장 (insert,update)
    void SaveItemAsync(
        uint64_t playerID,
        uint64_t itemUID,
        uint32_t itemID,
        uint32_t count,
        uint16_t slot,
        const std::vector<ItemOptionInfo>& options);

	// 아이템 삭제 쿼리
	void DeleteItemAsync(uint64_t playerID, uint64_t itemUID);

	
private:
	GameDB() = default;
	std::unique_ptr<DB::DBThreadPool> pool_;
};