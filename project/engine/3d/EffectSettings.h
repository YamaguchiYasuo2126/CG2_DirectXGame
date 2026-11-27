#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "Vector3.h"

// Editor とゲーム本編が共有する値。倍率 1.0 は従来の見た目を表す。
struct FireEffectSettings {
	float particlesPerSecond = 42.0f;
	float sizeScale = 1.0f;
	float riseSpeedScale = 1.0f;
	float lifetimeScale = 1.0f;
	Vector3 tint{ 1.0f, 1.0f, 1.0f };
};

struct ElectricEffectSettings {
	uint32_t firstSparkCount = 96;
	uint32_t secondSparkCount = 64;
	uint32_t finalSparkCount = 48;
	float sizeScale = 1.0f;
	float speedScale = 1.0f;
	float lifetimeScale = 1.0f;
	Vector3 tint{ 1.0f, 1.0f, 1.0f };
};

struct MagnetEffectSettings {
	uint32_t shardCount = 32;
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
bool SaveEffectSettings(const std::filesystem::path& path, const EffectSettings& settings, std::string& error);
