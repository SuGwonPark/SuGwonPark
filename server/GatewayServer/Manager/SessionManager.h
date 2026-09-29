#pragma once
#include "Network/ClientProxySession.h"

class SessionManager {
public:
	static SessionManager* GetInstance() {
		static SessionManager instance;
		return &instance;
	}

	void Register(uint64_t sessionId, ClientProxySessionRef session);
	void Unregister(uint64_t sessionId);
	ClientProxySessionRef FindSession(uint64_t sessionId);

private:
	SessionManager() = default;

private:
	std::mutex lock_;
	std::unordered_map<uint64_t, ClientProxySessionRef> sessions_;
};