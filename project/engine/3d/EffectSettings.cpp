#include "EffectSettings.h"

#include <Windows.h>

#include <cmath>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <locale>
#include <map>
#include <string_view>
#include <vector>

namespace {
struct JsonValue {
	enum class Kind { Number, Object, Array } kind = Kind::Number;
	double number = 0.0;
	std::map<std::string, JsonValue> members;
	std::vector<JsonValue> elements;
};

// 設定ファイルで使う数値・オブジェクト・配列だけを読む小さな JSON reader。
class JsonReader {
public:
	explicit JsonReader(std::string_view source) : source_(source) {}
	bool Read(JsonValue& value) {
		SkipSpace();
		if (!ReadValue(value)) { return false; }
		SkipSpace();
		return position_ == source_.size();
	}

private:
	void SkipSpace() {
		while (position_ < source_.size() &&
			(source_[position_] == ' ' || source_[position_] == '\n' ||
				source_[position_] == '\r' || source_[position_] == '\t')) { ++position_; }
	}
	bool Take(char expected) {
		SkipSpace();
		if (position_ >= source_.size() || source_[position_] != expected) { return false; }
		++position_;
		return true;
	}
	bool ReadString(std::string& result) {
		if (!Take('"')) { return false; }
		const size_t start = position_;
		while (position_ < source_.size() && source_[position_] != '"') {
			// この schema のキーは ASCII 固定で、エスケープ文字は必要ない。
			if (source_[position_] == '\\' || static_cast<unsigned char>(source_[position_]) < 0x20) {
				return false;
			}
			++position_;
		}
		if (position_ == source_.size()) { return false; }
		result.assign(source_.substr(start, position_ - start));
		++position_;
		return true;
	}
	bool ReadNumber(double& result) {
		SkipSpace();
		const size_t start = position_;
		if (position_ < source_.size() && source_[position_] == '-') { ++position_; }
		if (position_ == source_.size()) { return false; }
		if (source_[position_] == '0') { ++position_; }
		else {
			if (source_[position_] < '1' || source_[position_] > '9') { return false; }
			while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') { ++position_; }
		}
		if (position_ < source_.size() && source_[position_] == '.') {
			++position_;
			const size_t fractionStart = position_;
			while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') { ++position_; }
			if (fractionStart == position_) { return false; }
		}
		if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
			++position_;
			if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) { ++position_; }
			const size_t exponentStart = position_;
			while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') { ++position_; }
			if (exponentStart == position_) { return false; }
		}
		const char* first = source_.data() + start;
		const char* last = source_.data() + position_;
		const auto [end, error] = std::from_chars(first, last, result, std::chars_format::general);
		return error == std::errc{} && end == last && std::isfinite(result);
	}
	bool ReadValue(JsonValue& value) {
		SkipSpace();
		if (position_ == source_.size()) { return false; }
		if (source_[position_] == '{') {
			value.kind = JsonValue::Kind::Object;
			++position_;
			if (Take('}')) { return true; }
			do {
				std::string key;
				JsonValue member;
				if (!ReadString(key) || !Take(':') || !ReadValue(member)) { return false; }
				if (!value.members.emplace(std::move(key), std::move(member)).second) { return false; }
				if (Take('}')) { return true; }
			} while (Take(','));
			return false;
		}
		if (source_[position_] == '[') {
			value.kind = JsonValue::Kind::Array;
			++position_;
			if (Take(']')) { return true; }
			do {
				JsonValue element;
				if (!ReadValue(element)) { return false; }
				value.elements.push_back(std::move(element));
				if (Take(']')) { return true; }
			} while (Take(','));
			return false;
		}
		return ReadNumber(value.number);
	}
	std::string_view source_;
	size_t position_ = 0;
};

const JsonValue* Field(const JsonValue& object, const char* key) {
	if (object.kind != JsonValue::Kind::Object) { return nullptr; }
	const auto it = object.members.find(key);
	return it == object.members.end() ? nullptr : &it->second;
}
bool Number(const JsonValue& object, const char* key, double minimum, double maximum, float& out) {
	const JsonValue* value = Field(object, key);
	if (!value || value->kind != JsonValue::Kind::Number ||
		value->number < minimum || value->number > maximum) { return false; }
	out = static_cast<float>(value->number);
	return true;
}
bool Count(const JsonValue& object, const char* key, uint32_t maximum, uint32_t& out) {
	const JsonValue* value = Field(object, key);
	if (!value || value->kind != JsonValue::Kind::Number || value->number < 0.0 ||
		value->number > maximum || std::floor(value->number) != value->number) { return false; }
	out = static_cast<uint32_t>(value->number);
	return true;
}
bool Tint(const JsonValue& object, Vector3& out) {
	const JsonValue* value = Field(object, "tint");
	if (!value || value->kind != JsonValue::Kind::Array || value->elements.size() != 3) { return false; }
	float* channels[] = { &out.x, &out.y, &out.z };
	for (size_t i = 0; i < 3; ++i) {
		const JsonValue& channel = value->elements[i];
		if (channel.kind != JsonValue::Kind::Number || channel.number < 0.0 || channel.number > 1.0) { return false; }
		*channels[i] = static_cast<float>(channel.number);
	}
	return true;
}
void WriteTint(std::ostream& stream, const Vector3& tint) {
	stream << '[' << tint.x << ", " << tint.y << ", " << tint.z << ']';
}
} // namespace

bool LoadEffectSettings(const std::filesystem::path& path, EffectSettings& out, std::string& error) {
	std::ifstream file(path, std::ios::binary);
	if (!file) { error = "Could not open resources/effect-presets.json"; return false; }
	const std::string text(std::istreambuf_iterator<char>{ file }, {});
	JsonValue root;
	if (!JsonReader(text).Read(root)) { error = "Invalid effect preset JSON"; return false; }
	const JsonValue* fire = Field(root, "fire");
	const JsonValue* electric = Field(root, "electric");
	const JsonValue* magnet = Field(root, "magnet");
	const JsonValue* version = Field(root, "version");
	if (!fire || !electric || !magnet || !version || version->kind != JsonValue::Kind::Number || version->number != 1.0) {
		error = "Missing effect section or unsupported version"; return false;
	}
	// 全項目を一時データへ検証してから反映し、不正なJSONで一部だけ更新されるのを防ぐ。
	EffectSettings parsed;
	if (!Number(*fire, "particlesPerSecond", 0.0, 160.0, parsed.fire.particlesPerSecond) ||
		!Number(*fire, "sizeScale", 0.25, 3.0, parsed.fire.sizeScale) ||
		!Number(*fire, "riseSpeedScale", 0.2, 3.0, parsed.fire.riseSpeedScale) ||
		!Number(*fire, "lifetimeScale", 0.3, 3.0, parsed.fire.lifetimeScale) || !Tint(*fire, parsed.fire.tint) ||
		!Count(*electric, "firstSparkCount", 128, parsed.electric.firstSparkCount) ||
		!Count(*electric, "secondSparkCount", 96, parsed.electric.secondSparkCount) ||
		!Count(*electric, "finalSparkCount", 96, parsed.electric.finalSparkCount) ||
		!Number(*electric, "sizeScale", 0.25, 3.0, parsed.electric.sizeScale) ||
		!Number(*electric, "speedScale", 0.2, 3.0, parsed.electric.speedScale) ||
		!Number(*electric, "lifetimeScale", 0.3, 3.0, parsed.electric.lifetimeScale) || !Tint(*electric, parsed.electric.tint) ||
		!Count(*magnet, "shardCount", 64, parsed.magnet.shardCount) ||
		!Number(*magnet, "sizeScale", 0.25, 3.0, parsed.magnet.sizeScale) ||
		!Number(*magnet, "orbitRadiusScale", 0.5, 2.0, parsed.magnet.orbitRadiusScale) ||
		!Number(*magnet, "orbitSpeedScale", 0.2, 3.0, parsed.magnet.orbitSpeedScale) ||
		!Number(*magnet, "releaseSpeedScale", 0.2, 3.0, parsed.magnet.releaseSpeedScale) || !Tint(*magnet, parsed.magnet.tint)) {
		error = "Missing or out-of-range effect parameter"; return false;
	}
	out = parsed;
	error.clear();
	return true;
}

bool SaveEffectSettings(const std::filesystem::path& path, const EffectSettings& settings, std::string& error) {
	std::error_code filesystemError;
	std::filesystem::create_directories(path.parent_path(), filesystemError);
	if (filesystemError) { error = "Could not create preset directory"; return false; }
	// 既存設定を直接切り詰めず、同じ場所の一時ファイルへ完成形を書き出す。
	std::filesystem::path temporary = path;
	temporary += L".tmp";
	std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
	if (!file) { error = "Could not write effect preset"; return false; }
	// ロケールによる小数点の変化を防ぎ、JSONとして読める表記へ固定する。
	file.imbue(std::locale::classic());
	file << std::setprecision(8);
	file << "{\n  \"version\": 1,\n  \"fire\": {\n"
		<< "    \"particlesPerSecond\": " << settings.fire.particlesPerSecond << ",\n"
		<< "    \"sizeScale\": " << settings.fire.sizeScale << ",\n"
		<< "    \"riseSpeedScale\": " << settings.fire.riseSpeedScale << ",\n"
		<< "    \"lifetimeScale\": " << settings.fire.lifetimeScale << ",\n"
		<< "    \"tint\": ";
	WriteTint(file, settings.fire.tint);
	file << "\n  },\n  \"electric\": {\n"
		<< "    \"firstSparkCount\": " << settings.electric.firstSparkCount << ",\n"
		<< "    \"secondSparkCount\": " << settings.electric.secondSparkCount << ",\n"
		<< "    \"finalSparkCount\": " << settings.electric.finalSparkCount << ",\n"
		<< "    \"sizeScale\": " << settings.electric.sizeScale << ",\n"
		<< "    \"speedScale\": " << settings.electric.speedScale << ",\n"
		<< "    \"lifetimeScale\": " << settings.electric.lifetimeScale << ",\n"
		<< "    \"tint\": ";
	WriteTint(file, settings.electric.tint);
	file << "\n  },\n  \"magnet\": {\n"
		<< "    \"shardCount\": " << settings.magnet.shardCount << ",\n"
		<< "    \"sizeScale\": " << settings.magnet.sizeScale << ",\n"
		<< "    \"orbitRadiusScale\": " << settings.magnet.orbitRadiusScale << ",\n"
		<< "    \"orbitSpeedScale\": " << settings.magnet.orbitSpeedScale << ",\n"
		<< "    \"releaseSpeedScale\": " << settings.magnet.releaseSpeedScale << ",\n"
		<< "    \"tint\": ";
	WriteTint(file, settings.magnet.tint);
	file << "\n  }\n}\n";
	file.close();
	if (!file) { error = "Could not finish writing effect preset"; std::filesystem::remove(temporary, filesystemError); return false; }
	// closeまで成功した場合だけ置換する。置換失敗時も元の設定ファイルは保持する。
	if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		error = "Could not replace effect preset file";
		std::filesystem::remove(temporary, filesystemError);
		return false;
	}
	error.clear();
	return true;
}
