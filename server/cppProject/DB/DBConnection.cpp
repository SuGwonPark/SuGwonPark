#include "pch.h"
#include "DB/DBConnection.h"

DBConnection::DBConnection() {}

DBConnection::~DBConnection() {
	Disconnect();
}

bool DBConnection::Connect(const std::string& host,
	const std::string& user,
	const std::string& password,
	const std::string& database,
	unsigned int port) {
	try {
		// 세션 생성 (연결)
		session_ = std::make_unique<mysqlx::Session>(
			mysqlx::SessionOption::HOST, host,
			mysqlx::SessionOption::PORT, port,
			mysqlx::SessionOption::USER, user,
			mysqlx::SessionOption::PWD, password
		);

		// DB 선택
		schema_ = std::make_unique<mysqlx::Schema>(
			session_->getSchema(database)
		);

		connected_ = true;
		std::cout << "[DB] 연결 성공 : " << database << std::endl;
		return true;
	}
	catch (const mysqlx::Error& e) {
		std::cout << "[DB] 연결 실패 : " << e.what() << std::endl;
		connected_ = false;
		return false;
	}
}

void DBConnection::Disconnect() {
	if (connected_ && session_) {
		session_->close();
		connected_ = false;
		std::cout << "[DB] 연결 종료" << std::endl;
	}
}

mysqlx::SqlResult DBConnection::Query(const std::string& sql) {
	try {
		return session_->sql(sql).execute();
	}
	catch (const mysqlx::Error& e) {
		std::cout << "[DB] 쿼리 실패 : " << e.what() << std::endl;
		throw;
	}
}

bool DBConnection::Execute(const std::string& sql) {
	try {
		session_->sql(sql).execute();
		return true;
	}
	catch (const mysqlx::Error& e) {
		std::cout << "[DB] 실행 실패 : " << e.what() << std::endl;
		return false;
	}
}