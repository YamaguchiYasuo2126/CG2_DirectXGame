#pragma once

#include <fstream>
#include <string>

// ファイルとデバッグ出力へログを書き出すクラス。
class Logger {
public:
	// logsフォルダに実行日時付きのログファイルを開く。
	void Initialize();
	void Finalize();
	// ファイルとVisual Studioのデバッグ出力へ同じメッセージを書き出す。
	void Log(const std::string& message);

private:
	std::ofstream logStream_;
};
