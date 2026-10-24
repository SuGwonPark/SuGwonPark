#pragma once
#include <cstdint>
#include "VarEnum.h"

// 패킷 필드에서 공통으로 쓰는 타입 모음
//  - 패킷 번호: PacketID.h
//  - 요청/응답 패킷: Packets.h
//  - 패킷 안에 들어가는 값/데이터 묶음: 이 파일

// 클라이언트와 약속한 프로토콜 한계값. 바꾸면 클라이언트도 같이 바꿔야 한다
namespace Net {
	inline constexpr uint16_t MAX_CLIENT_PACKET_SIZE = 4096; // 클라이언트 패킷 최대 크기
}

// 클라이언트와 공유하는 게임 규칙값
namespace Game {
	inline constexpr int MAX_CHARACTER_SLOT = 6; // 계정당 캐릭터 슬롯 수
}


// 아래 구조체는 패킷 바이트에 그대로 복사되므로 반드시 1바이트 정렬
#pragma pack(push, 1)

struct CharacterSummary {        // 캐릭터 목록 한 줄
	uint64_t playerID;
	uint64_t exp;
	char     name[50];
};

struct PlayerInfo {              // 다른 유저에게 보여줄 최소 정보
	uint64_t playerID;
	char     name[50];
	float    x, y, z;
	int32_t  hp;
};

struct MonsterInfo {
	uint64_t monsterUID;
	uint32_t monsterID;
	float    x, y, z;
	int32_t  hp;
	int32_t  maxHp;
};

struct SkillHit {                // 스킬에 맞은 대상 1건
	ObjectType targetType;
	uint64_t   targetUID;
	int32_t    damage;
	int32_t    targetHp;         // 맞은 뒤 남은 HP
};

#pragma pack(pop)
