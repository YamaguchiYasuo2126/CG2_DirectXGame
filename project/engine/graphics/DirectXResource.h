#pragma once

#include <cstddef>
#include <cstdint>
#include <d3d12.h>
#include <string>
#include <wrl.h>

#include "externals/DirectXTex/DirectXTex.h"

// DirectXリソース作成の共通処理をまとめた補助関数群です。
namespace DirectXResource {
	// CPUからMapして書き込めるUPLOADヒープのバッファ。サイズの単位はバイト。
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);
	// shaderVisibleは、シェーダーから参照するCBV/SRV/UAV等のヒープで指定する。
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
		ID3D12Device* device,
		D3D12_DESCRIPTOR_HEAP_TYPE heapType,
		UINT numDescriptors,
		bool shaderVisible);
	// WICで画像を読み、sRGBとして扱うMip列を生成する。パスは作業ディレクトリ基準。
	DirectX::ScratchImage LoadTexture(const std::string& filePath);
	// 画像メタデータに合わせて、CPU書き込み可能なテクスチャを確保する。
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);
	// WriteToSubresourceで各Mipを転送する。CPU書き込み可能なtextureが前提。
	void UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages);
	// 深度テスト用の画面サイズのテクスチャを、DEFAULTヒープに確保する。
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height);
}
