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

private:
	SessionManager() = default;

private:
	std::mutex lock_;
	std::unordered_map<uint64_t, ClientProxySessionRef> sessions_;
	std::unordered_map<uint64_t, uint64_t> userToSession_;
};