#include "pch.h"
#include "Manager/SessionManager.h"

void SessionManager::Register(uint64_t sessionID, ClientProxySessionRef session) {
	std::lock_guard<std::mutex> lock(lock_);
	sessions_[sessionID] = session;
}

void SessionManager::Unregister(uint64_t sessionID, uint64_t userUID) {
	std::lock_guard<std::mutex> lock(lock_);
	sessions_.erase(sessionID);

	// 계정 바인드 (내것만 삭제)
	if (userUID != 0) {
		auto it = userToSession_.find(userUID);
		// 중복으로 들어올 경우 처음에 등록된 세션이 아니면 삭제 안함
		if (it != userToSession_.end() && it->second == sessionID) {
			userToSession_.erase(it);
		}
	}
}

ClientProxySessionRef SessionManager::FindSession(uint64_t sessionID) {
	std::lock_guard<std::mutex> lock(lock_);
	auto it = sessions_.find(sessionID);
	return (it != sessions_.end()) ? it->second : nullptr;
}

bool SessionManager::TryBindUser(uint64_t userUID, uint64_t sessionID, ClientProxySessionRef& existing)
{
	std::lock_guard<std::mutex> lock(lock_);

	auto it = userToSession_.find(userUID);
	// 중복 로그인시
	if (it != userToSession_.end() && it->second != sessionID) {
		auto s = sessions_.find(it->second);
		// 세션 정보가 있으면 그대로 넘겨줌 없으면 nullptr
		existing = (s != sessions_.end()) ? s->second : nullptr;
		return false;
	}

	userToSession_[userUID] = sessionID;

	return true;
}
