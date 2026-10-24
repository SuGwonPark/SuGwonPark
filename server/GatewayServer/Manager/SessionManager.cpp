#include "pch.h"
#include "Manager/SessionManager.h"
#include "ServerCore/Protocol/InternalPackets.h"  

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
		// 기존 접속중인 유저의 session을 찾는다
		auto s = sessions_.find(it->second);
		if (s != sessions_.end()) {
			// 실제로 살아 있는 세션이 이 계정을 잡고 있음 → 중복 로그인
			existing = s->second;
			return false;
		}
	}

	userToSession_[userUID] = sessionID;

	return true;
}

std::vector<ClientProxySessionRef> SessionManager::CollectSessionsInZones(const std::unordered_set<uint64_t>& zoneKeys) {
	std::vector<ClientProxySessionRef> result;

	std::lock_guard<std::mutex> lock(lock_);
	// 모든 세션
	for (auto& [sessionID, session] : sessions_) {
		// 상태값이 InGame인 세션만 찾는다
		if (session->GetState() != SessionState::InGame) continue;

		uint64_t key = MakeZoneKey(session->GetCurrentZoneID(), session->GetCurrentChannelID());
		if (zoneKeys.count(key)) result.push_back(session);
	}
	return result;
}