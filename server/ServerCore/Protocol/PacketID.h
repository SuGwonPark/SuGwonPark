#pragma once
#include <cstdint>

// 클라이언트 <-> 서버 패킷 번호
// 대역: 1000 이동/전투 | 2000 존 | 3000 채팅 | 4000 시스템 | 5000 로그인 | 6000 로비 | 7000 아이템
enum PacketID : uint16_t
{
    // 이동 및 전투 (1000~)
    PKT_C_MOVE = 1001,
    PKT_C_ATTACK = 1002,
    PKT_C_SKILL_CAST = 1003,
    PKT_S_MOVE = 1004,
    PKT_S_ATTACK = 1005,
    PKT_S_EXP = 1006,
    PKT_S_SKILL_CAST = 1007,  // 스킬 결과 (브로드캐스트)
    PKT_S_SKILL_FAIL = 1008,  // 스킬 실패 (시전자에게만)
    PKT_S_PLAYER_STAT = 1009, // 플레이어 HP/MP (브로드캐스트)
    PKT_S_LEVEL_UP = 1010,    // 레벨업 (주변 브로드캐스트)
    PKT_S_MY_STAT = 1011,     // 내 전체 스탯 (나에게만)

    // 몬스터 (1100~)
    PKT_S_MONSTER_SPAWN = 1101,   // 몬스터 생성
    PKT_S_MONSTER_DESPAWN = 1102, // 몬스터 제거 (사망 등, 3단계에서 사용)
    PKT_S_MONSTER_MOVE = 1103,    // 몬스터 이동
    PKT_S_MONSTER_HP = 1104,      // 몬스터 체력 상태

    // 존 (2000~)
    PKT_S_ZONE_LEAVE_COMPLETED = 2001,
    PKT_S_ENTER_GAME = 2002, // 내 입장 결과
    PKT_S_SPAWN = 2003,      // 다른 플레이어 등장 (목록)
    PKT_S_DESPAWN = 2004,    // 다른 플레이어 퇴장
    PKT_C_PORTAL = 2005,
    PKT_S_ZONE_CHANGE = 2006,

    // 채팅 (3000~)
    PKT_C_CHAT = 3001,
    PKT_C_WHISPER = 3002,

    // 시스템 (4000~)
    PKT_C_READY_TO_SPAWN = 4001,
    PKT_C_REVIVE = 4002, // 부활 요청
    PKT_S_REVIVE = 4003, // 부활 (브로드캐스트)

    // 로그인 (5000~)
    PKT_C_LOGIN = 5001,
    PKT_S_LOGIN = 5002,

    // 로비 (6000~)
    PKT_C_CHARACTER_LIST = 6001,
    PKT_S_CHARACTER_LIST = 6002,
    PKT_C_CREATE_CHARACTER = 6003,
    PKT_S_CREATE_CHARACTER = 6004,
    PKT_C_SELECT_CHARACTER = 6005,
    PKT_S_SELECT_CHARACTER = 6006,

    // 아이템 (7000~)
    PKT_C_ENHANCE_ITEM = 7001,
    PKT_S_ENHANCE_ITEM = 7002,
    PKT_S_INVENTORY = 7003,        // 인벤토리
    PKT_S_INVENTORY_UPDATE = 7004, // 바뀐 칸
    PKT_S_GOLD = 7005,             // 골드 변화
    PKT_S_DROP_SPAWN = 7006,       // 바닥 아이템 등장 (목록)
    PKT_S_DROP_DESPAWN = 7007,     // 바닥 아이템 사라짐 (주움 / 시간 지남 / 시야 밖)
    PKT_C_PICKUP = 7008,           // 줍기 요청
    PKT_S_PICKUP_FAIL = 7009,      // 줍기 실패 (나에게만)
    PKT_C_USE_ITEM = 7010,         // 아이템 사용
    PKT_S_USE_ITEM_FAIL = 7011,    // 아이템 사용 실패 (나에게만)
    PKT_C_DESTROY_ITEM = 7012,     // 아이템 버리기
    PKT_C_EQUIP = 7013,            // 장비 착용 (itemUID)
    PKT_C_UNEQUIP = 7014,          // 장비 해제 (EquipSlot)
    PKT_S_EQUIP_FAIL = 7015,
    PKT_S_EQUIPMENT = 7016,     // 장착 칸 목록 (입장: 전체 / 변경: 해당 칸)
    PKT_S_EQUIP_CHANGED = 7017, // 외형 변경 (주변 브로드캐스트)
};
