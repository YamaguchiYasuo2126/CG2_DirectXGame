#include "Logger.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <format>

void Logger::Initialize() {
	// 実行ごとに別ファイルへ出せるよう、日時入りのログファイルを作ります。
	std::filesystem::create_directory("logs");

	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> nowSeconds =
		std::chrono::time_point_cast<std::chrono::seconds>(now);
	std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	std::string logFilePath = std::string("logs/") + dateString + ".log";
	logStream_.open(logFilePath);
}

void Logger::Finalize() {
	// ログファイルを閉じて、書き込み内容を確定させます。
	if (logStream_.is_open()) {
		logStream_.close();
	}
}

void Logger::Log(const std::string& message) {
	// ファイルとVisual Studioの出力ウィンドウの両方へ出力します。
	if (logStream_.is_open()) {
		logStream_ << message << std::endl;
	}
	OutputDebugStringA(message.c_str());
}
