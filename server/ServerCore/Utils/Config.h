#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <iostream>

// 서버 실행 환경 설정. Config/server.ini 에서 읽는다 (GatewayServer / ZoneServer 공용)
struct ServerConfig {
	std::string gatewayHost = "127.0.0.1";   // Zone 서버가 접속할 Gateway 주소
	uint16_t    clientPort = 8080;          // 클라이언트 -> Gateway
	uint16_t    internalPort = 9000;          // Zone -> Gateway (내부)
};

namespace ConfigDetail {
	inline std::string Trim(const std::string& s) {
		const char* ws = " \t\r\n";
		size_t b = s.find_first_not_of(ws);
		if (b == std::string::npos) return "";
		size_t e = s.find_last_not_of(ws);
		return s.substr(b, e - b + 1);
	}

	// 실행 위치(VS 디버깅 = 프로젝트 폴더, 직접 실행 = x64\Debug)가 달라도 찾을 수 있게
	// 현재 폴더부터 위로 3단계까지 Config/<fileName> 을 찾는다
	inline std::filesystem::path FindConfigFile(const std::string& fileName) {
		std::filesystem::path dir = std::filesystem::current_path();
		for (int i = 0; i <= 3; ++i) {
			std::filesystem::path candidate = dir / "Config" / fileName;
			std::error_code ec;
			if (std::filesystem::exists(candidate, ec)) return candidate;
			if (!dir.has_parent_path() || dir.parent_path() == dir) break;
			dir = dir.parent_path();
		}
		return {};
	}

	inline void ReadPort(const std::unordered_map<std::string, std::string>& kv, const char* key, uint16_t& out) {
		auto it = kv.find(key);
		if (it == kv.end()) return;
		try {
			int v = std::stoi(it->second);
			if (v > 0 && v <= 65535) { out = static_cast<uint16_t>(v); return; }
		}
		catch (...) {}
		std::cerr << "[Config] 잘못된 포트 값 " << key << " = " << it->second << ", 기본값 " << out << " 사용" << std::endl;
	}
}

// 형식: key = value  ('#' 또는 ';'로 시작하는 줄은 주석)
// 파일이 없거나 값이 빠져 있으면 ServerConfig의 기본값을 쓴다
inline ServerConfig LoadServerConfig(const std::string& fileName = "server.ini") {
	ServerConfig cfg;

	std::filesystem::path path = ConfigDetail::FindConfigFile(fileName);
	if (path.empty()) {
		std::cerr << "[Config] Config/" << fileName << " 파일을 찾지 못해 기본값을 사용합니다." << std::endl;
		return cfg;
	}

	std::ifstream in(path);
	std::unordered_map<std::string, std::string> kv;
	std::string line;
	bool firstLine = true;
	while (std::getline(in, line)) {
		if (firstLine && line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);   // UTF-8 BOM 제거
		firstLine = false;

		line = ConfigDetail::Trim(line);
		if (line.empty() || line[0] == '#' || line[0] == ';') continue;

		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		kv[ConfigDetail::Trim(line.substr(0, eq))] = ConfigDetail::Trim(line.substr(eq + 1));
	}

	if (auto it = kv.find("gateway.host"); it != kv.end() && !it->second.empty()) cfg.gatewayHost = it->second;
	ConfigDetail::ReadPort(kv, "gateway.client_port", cfg.clientPort);
	ConfigDetail::ReadPort(kv, "gateway.internal_port", cfg.internalPort);

	std::cout << "[Config] " << path.string() << " 로드 (host=" << cfg.gatewayHost
		<< ", client_port=" << cfg.clientPort << ", internal_port=" << cfg.internalPort << ")" << std::endl;
	return cfg;
}
