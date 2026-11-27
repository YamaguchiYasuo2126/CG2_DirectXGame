#pragma once

#include <array>
#include <cstdint>
#include <random>

#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>

#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "WeatherConditions.h"

class DirectXCommon;
class ShaderCompiler;

// 障害物の演出とは独立して、雨・雪・砂嵐を描画する。
class WeatherSystem {
public:
	// 雨・雪・砂嵐で使うPSO、頂点、インスタンスバッファをまとめて作成する。
	void Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler);
	// deltaTimeは秒。カメラ周辺で粒を発生させ、描画用の行列と色を更新する。
	void Update(float deltaTime, const Vector3& cameraPosition,
		const Matrix4x4& cameraWorld, const Matrix4x4& viewProjection);
	void Draw() const;
	void DrawImGuiControls();
	// 粒の描画設定とは分けて、他の演出へ風速と雨の強さだけを公開する。
	WeatherConditions GetConditions() const;
	void Finalize();

private:
	enum class Mode { Off, Rain, Snow, Sandstorm };
	static constexpr uint32_t kMaxParticles = 4096;

	struct WeatherParticle {
		Vector3 position{};
		float speedScale = 1.0f;
		float size = 1.0f;
		float alpha = 0.4f;
		float age = 0.0f;
		float flutterPhase = 0.0f;
		bool dustCloud = false;
		bool alive = false;
	};

	struct WeatherVertex {
		Vector4 position;
		Vector2 uv;
	};

	// 天候用VSのStructuredBufferに対応するGPU転送データ。
	struct WeatherInstance {
		Matrix4x4 WVP;
		Vector4 color;
	};

	struct SandHazeConstants {
		float time = 0.0f;
		float opacity = 0.0f;
		float flowX = 0.0f;
		float flowZ = 0.0f;
	};

	DirectXCommon* dxCommon_ = nullptr;
	Mode mode_ = Mode::Off;
	// 発生頻度は粒/秒、落下・移動速度はワールド単位/秒。
	float rainPerSecond_ = 500.0f;
	float rainFallSpeed_ = 8.0f;
	float snowPerSecond_ = 120.0f;
	float snowFallSpeed_ = 1.0f;
	float sandPerSecond_ = 1800.0f;
	float sandSpeed_ = 6.0f;
	float sandDirection_ = 0.0f; // XZ平面内の進行角（ラジアン）。
	float sandVeilOpacity_ = 0.9f;
	float sandTime_ = 0.0f;
	float windX_ = 0.0f;
	float windZ_ = 0.0f;
	float spawnCredit_ = 0.0f; // 発生数の小数部分を次フレームへ繰り越す。
	uint32_t instanceCount_ = 0;
	uint32_t spawnCursor_ = 0; // 次に空きスロットを探し始める位置。配列末尾で循環する。
	std::mt19937 randomEngine_{ std::random_device{}() };
	std::array<WeatherParticle, kMaxParticles> particles_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> instanceResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> sandHazeResource_;
	// Map先のポインタは各Resourceが所有するメモリを指し、deleteしない。
	WeatherInstance* instanceData_ = nullptr;
	SandHazeConstants* sandHazeData_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_GPU_DESCRIPTOR_HANDLE instanceSrvHandle_{};
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> rainPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> snowPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> sandPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> sandHazePipelineState_;
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob_;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob_;
	Microsoft::WRL::ComPtr<IDxcBlob> snowPixelShaderBlob_;
	Microsoft::WRL::ComPtr<IDxcBlob> sandPixelShaderBlob_;
	Microsoft::WRL::ComPtr<IDxcBlob> sandHazeVertexShaderBlob_;
	Microsoft::WRL::ComPtr<IDxcBlob> sandHazePixelShaderBlob_;
};
