#pragma once
#include "Network/ClientProxySession.h"   // ClientProxySessionRef
#include "Protocol/Packets.h"

class Account {
public:
	static Account* GetInstance() {
		static Account instance;
		return &instance;
	}

	void Login(ClientProxySessionRef session, const REQ_LoginPacket* pkt);

	// 캐릭터 리스트
	void SendCharacterList(ClientProxySessionRef session);
	// 캐릭터 선택
	void SelectCharacter(ClientProxySessionRef session);


private:
	Account() = default;
};