#pragma once
#include "pch.h"

class DBConnection {
public:
	DBConnection();
	~DBConnection();

	bool Connect(const std::string& host,
		const std::string& user,
		const std::string& password,
		const std::string& database,
		unsigned int port = 33060);

	void Disconnect();
	bool IsConnected() const { return connected_; }

	// 조회 (SELECT)
	mysqlx::SqlResult Query(const std::string& sql);

	// 실행 (INSERT, UPDATE, DELETE)
	bool Execute(const std::string& sql);

private:
	std::unique_ptr<mysqlx::Session> session_;
	std::unique_ptr<mysqlx::Schema>  schema_;
	bool connected_ = false;
};
