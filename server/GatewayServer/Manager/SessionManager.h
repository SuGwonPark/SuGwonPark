#pragma once
#include "Network/ClientProxySession.h"

class SessionManager {
public:
	static SessionManager* GetInstance() {
		static SessionManager instance;
		return &instance;
	}

	void Register(uint64_t sessionID, ClientProxySessionRef session);
	void Unregister(uint64_t sessionID, uint64_t userUID);
	ClientProxySessionRef FindSession(uint64_t sessionID);

	// 로그인 성공 시 계정을 이 세션으로 묶는다
	// 이미 다른 세션이 있으면 false
	bool TryBindUser(uint64_t userUID, uint64_t sessionID, ClientProxySessionRef& existing);

	// 주어진 Zone 키들에 있는(InGame) 클라이언트 세션을 모아 돌려준다
	std::vector<ClientProxySessionRef> CollectSessionsInZones(const std::unordered_set<uint64_t>& zoneKeys);
private:
	SessionManager() = default;

private:
	std::mutex lock_;
	std::unordered_map<uint64_t, ClientProxySessionRef> sessions_;
	std::unordered_map<uint64_t, uint64_t> userToSession_;
};