#pragma once

#include <fstream>
#include <string>

// ファイルとデバッグ出力へログを書き出すクラスです。
class Logger {
public:
	void Initialize();
	void Finalize();
	void Log(const std::string& message);

private:
	std::ofstream logStream_;
};
