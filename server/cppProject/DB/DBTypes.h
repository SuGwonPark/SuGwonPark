#pragma once
#include <pch.h>

namespace DB
{
	// DB 스레드가 넘겨받아 실행할 작업 단위 (함수 객체)
	using DBTask = std::function<void(mysqlx::Session&)>;

	// (선택) DB 조회가 완료된 후 Game Thread에서 다룰 결과 콜백 타입
	using DBResultCallback = std::function<void()>;
}