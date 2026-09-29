#pragma once
#include "Network/ClientProxySession.h"   // ClientProxySessionRef
#include "Network/Protocol.h"

class Account {
public:
	static Account* GetInstance() {
		static Account instance;
		return &instance;
	}

	void Login(ClientProxySessionRef session, const REQ_LoginPacket* pkt);


private:
	Account() = default;
};