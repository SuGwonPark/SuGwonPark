#pragma once
// ServerCore/Utils/CsvReader.h
// Data/*.csv 읽기 공용 함수
//  - 1행은 헤더, 2행부터 데이터
//  - "0,0,0" 처럼 따옴표로 감싼 칸은 안의 쉼표를 나누지 않는다 ("" 는 따옴표 문자)
//  - UTF-8 BOM, CRLF, 빈 줄 처리
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <iostream>

using CsvRow = std::vector<std::string>;

namespace Csv {

	inline std::string Trim(const std::string& s) {
		const char* ws = " \t\r\n";
		size_t b = s.find_first_not_of(ws);
		if (b == std::string::npos) return "";
		size_t e = s.find_last_not_of(ws);
		return s.substr(b, e - b + 1);
	}

	// 한 줄 → 칸 목록 (따옴표 처리)
	inline CsvRow SplitLine(const std::string& line) {
		CsvRow row;
		std::string cell;
		bool inQuotes = false;

		for (size_t i = 0; i < line.size(); ++i) {
			char c = line[i];
			if (inQuotes) {
				if (c == '"') {
					if (i + 1 < line.size() && line[i + 1] == '"') { cell += '"'; ++i; }   // "" → "
					else inQuotes = false;
				}
				else cell += c;
			}
			else if (c == '"') inQuotes = true;
			else if (c == ',') { row.push_back(Trim(cell)); cell.clear(); }
			else cell += c;
		}
		row.push_back(Trim(cell));
		return row;
	}

	// 실행 위치(VS 디버깅 = 프로젝트 폴더, 직접 실행 = x64\Debug)가 달라도 찾을 수 있게
	// 현재 폴더부터 위로 3단계까지 Data/<fileName> 을 찾는다. 없으면 빈 path
	inline std::filesystem::path FindDataFile(const std::string& fileName) {
		std::filesystem::path dir = std::filesystem::current_path();
		for (int i = 0; i <= 3; ++i) {
			std::filesystem::path candidate = dir / "Data" / fileName;
			std::error_code ec;
			if (std::filesystem::exists(candidate, ec)) return candidate;
			if (!dir.has_parent_path() || dir.parent_path() == dir) break;
			dir = dir.parent_path();
		}
		return {};
	}

	// ===== 칸 변환. 실패하면 예외 (ForEachRow가 잡아서 몇 번째 줄인지 출력) =====
	inline uint64_t ToU64(const std::string& s) {
		if (s.empty() || s[0] == '-') throw std::invalid_argument("0 이상의 정수가 아님: '" + s + "'");
		size_t pos = 0;
		uint64_t v = std::stoull(s, &pos);
		if (pos != s.size()) throw std::invalid_argument("정수가 아님: '" + s + "'");
		return v;
	}

	inline uint32_t ToU32(const std::string& s) {
		uint64_t v = ToU64(s);
		if (v > UINT32_MAX) throw std::out_of_range("uint32 범위 초과: '" + s + "'");
		return static_cast<uint32_t>(v);
	}

	inline int32_t ToI32(const std::string& s) {
		size_t pos = 0;
		int v = std::stoi(s, &pos);
		if (pos != s.size()) throw std::invalid_argument("정수가 아님: '" + s + "'");
		return v;
	}

	inline float ToFloat(const std::string& s) {
		size_t pos = 0;
		float v = std::stof(s, &pos);
		if (pos != s.size()) throw std::invalid_argument("실수가 아님: '" + s + "'");
		return v;
	}

	// "x,y,z" → 실수 3개
	inline void ToVec3(const std::string& s, float& x, float& y, float& z) {
		CsvRow parts = SplitLine(s);
		if (parts.size() != 3) throw std::invalid_argument("\"x,y,z\" 형식이 아님: '" + s + "'");
		x = ToFloat(parts[0]);
		y = ToFloat(parts[1]);
		z = ToFloat(parts[2]);
	}

	// Data/<fileName> 을 읽어 데이터 줄마다 parse(row) 호출
	//  - 헤더/데이터 줄의 칸 수가 columnCount와 다르면 실패
	//  - parse 안에서 예외가 나면 "파일 line N: 이유" 출력 후 실패
	template<typename ParseFn>
	bool ForEachRow(const std::string& fileName, size_t columnCount, ParseFn parse) {
		const std::filesystem::path path = FindDataFile(fileName);
		if (path.empty()) {
			std::cerr << "[Csv] Data/" << fileName << " 파일을 찾지 못함" << std::endl;
			return false;
		}

		std::ifstream file(path);
		if (!file.is_open()) {
			std::cerr << "[Csv] 파일 열기 실패: " << path.string() << std::endl;
			return false;
		}

		std::string line;
		size_t lineNo = 0;
		bool headerDone = false;

		while (std::getline(file, line)) {
			++lineNo;
			if (lineNo == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);   // UTF-8 BOM
			if (!line.empty() && line.back() == '\r') line.pop_back();                        // CRLF
			if (Trim(line).empty()) continue;

			CsvRow row = SplitLine(line);

			if (!headerDone) {
				headerDone = true;
				if (row.size() != columnCount) {
					std::cerr << "[Csv] " << fileName << " 헤더 칸 수 " << row.size()
						<< " (코드는 " << columnCount << "칸 기준)" << std::endl;
					return false;
				}
				continue;
			}

			if (row.size() != columnCount) {
				std::cerr << "[Csv] " << fileName << " line " << lineNo << ": 칸 수 " << row.size()
					<< " (필요 " << columnCount << ")" << std::endl;
				return false;
			}

			try {
				parse(row);
			}
			catch (const std::exception& e) {
				std::cerr << "[Csv] " << fileName << " line " << lineNo << ": " << e.what() << std::endl;
				return false;
			}
		}

		if (!headerDone) {
			std::cerr << "[Csv] " << fileName << " 비어 있음 (헤더 없음)" << std::endl;
			return false;
		}
		return true;
	}
}
