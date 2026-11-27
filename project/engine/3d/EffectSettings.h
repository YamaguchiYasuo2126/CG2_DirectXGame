#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "Vector3.h"

// Editor とゲーム本編が共有する値。倍率 1.0 は従来の見た目を表す。
struct FireEffectSettings {
	float particlesPerSecond = 42.0f; // 炎1か所あたりの発生数/秒。雨の消火進行に応じて減少。
	float sizeScale = 1.0f;
	float riseSpeedScale = 1.0f;
	float lifetimeScale = 1.0f;
	Vector3 tint{ 1.0f, 1.0f, 1.0f }; // 基本色へ乗算するRGB（各成分0～1）。
};

struct ElectricEffectSettings {
	// 放電開始・2段目・最後の閃光に合わせて発生させる火花数。
	uint32_t firstSparkCount = 96;
	uint32_t secondSparkCount = 64;
	uint32_t finalSparkCount = 48;
	float sizeScale = 1.0f;
	float speedScale = 1.0f;
	float lifetimeScale = 1.0f;
	Vector3 tint{ 1.0f, 1.0f, 1.0f };
};

struct MagnetEffectSettings {
	uint32_t shardCount = 32; // 磁力障害物1つを構成する破片の数。
	float sizeScale = 1.0f;
	float orbitRadiusScale = 1.0f;
	float orbitSpeedScale = 1.0f;
	float releaseSpeedScale = 1.0f;
	Vector3 tint{ 1.0f, 1.0f, 1.0f };
};

struct EffectSettings {
	FireEffectSettings fire;
	ElectricEffectSettings electric;
	MagnetEffectSettings magnet;
};

// 読み込みに失敗しても out は変更しない。error には UI 用の理由を入れる。
bool LoadEffectSettings(const std::filesystem::path& path, EffectSettings& out, std::string& error);
// 一時ファイルへ書き終えてから保存先を置換し、書き込み途中の設定を残さないようにする。
bool SaveEffectSettings(const std::filesystem::path& path, const EffectSettings& settings, std::string& error);
