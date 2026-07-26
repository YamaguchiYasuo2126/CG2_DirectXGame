#pragma once

#include <Windows.h>
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

class Logger;

// DirectX 12の初期化、フレーム開始/終了、共通Descriptorを担当する
class DirectXCommon {
public:
	void Initialize(HWND hwnd, int32_t width, int32_t height, Logger* logger);
	void Finalize();
	void BeginFrame();
	void EndFrame();

	ID3D12Device* GetDevice() const { return device_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
	ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return srvDescriptorHeap_.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle(uint32_t index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle(uint32_t index) const;
	
	// 特定のインデックスのDescriptorHandleを取得する汎用関数
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) const;

	DXGI_FORMAT GetRenderTargetFormat() const { return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; }
	uint32_t GetBackBufferCount() const { return kBackBufferCount; }
	int32_t GetWidth() const { return width_; }
	int32_t GetHeight() const { return height_; }

private:
	// SwapChainのバックバッファ数。現在はダブルバッファ
	static constexpr uint32_t kBackBufferCount = 2;

	// Initializeから呼ばれるDirectX初期化処理
	void EnableDebugLayer();
	void CreateDevice();
	void SetupDebugInfoQueue();
	void CreateCommandObjects();
	void CreateSwapChain(HWND hwnd);
	void CreateDescriptorHeaps();
	void CreateRenderTargetViews();
	void CreateDepthStencilView();
	void CreateFence();
	void CreateViewportAndScissor();
	void WaitForGpu();
	void ReportLiveObjects();

	Logger* logger_ = nullptr;
	int32_t width_ = 0;
	int32_t height_ = 0;

	// DescriptorSizeを保存しておく変数
	uint32_t descriptorSizeSRV_ = 0;
	uint32_t descriptorSizeRTV_ = 0;
	uint32_t descriptorSizeDSV_ = 0;

	// DirectXの各種COMオブジェクトです。ComPtrで自動的にReleaseされます。
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController_;
#endif
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter_;
	Microsoft::WRL::ComPtr<ID3D12Device> device_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kBackBufferCount> swapChainResources_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kBackBufferCount> rtvHandles_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};
};
