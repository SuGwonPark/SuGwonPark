#include "pch.h"
#include "Manager/SessionManager.h"

void SessionManager::Register(uint64_t sessionId, ClientProxySessionRef session) {
	std::lock_guard<std::mutex> lock(lock_);
	sessions_[sessionId] = session;
}

void SessionManager::Unregister(uint64_t sessionId) {
	std::lock_guard<std::mutex> lock(lock_);
	sessions_.erase(sessionId);
}

ClientProxySessionRef SessionManager::FindSession(uint64_t sessionId) {
	std::lock_guard<std::mutex> lock(lock_);
	auto it = sessions_.find(sessionId);
	return (it != sessions_.end()) ? it->second : nullptr;
}