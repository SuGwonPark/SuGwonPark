#pragma once

// 서버 시작 시 Data/*.csv 전부 로드 + 테이블끼리 참조 검증
// io.run() 전에 1번만 호출. 이후 각 Template은 읽기 전용이라 락 없이 어느 스레드에서 읽어도 된다
class Template
{
public:
	static bool LoadTemplate();
};
