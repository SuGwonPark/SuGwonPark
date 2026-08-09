#pragma once
#include <pch.h>

namespace DB
{
	// DB 스레드가 넘겨받아 실행할 작업 단위 (함수 객체)
	using DBTask = std::function<void(mysqlx::Session&)>;
}