#pragma once

#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;
class ShaderCompiler;

// 全Spriteで共有する描画ルールを所有する。頂点・座標・色は各Spriteが持つ。
class SpriteRenderer {
public:
    SpriteRenderer() = default;
    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    // 描画基盤は借用し、ルートシグネチャとPSOを一度だけ作成する。
    void Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler);
    void Finalize();

    // BeginFrameの後、連続して描くSprite群の直前に一度呼ぶ。
    // 途中で3DやImGuiを描いた場合は、次のSprite群の前に再設定する。
    void BeginDraw() const;

    DirectXCommon* GetDirectXCommon() const { return dxCommon_; }

private:
    void CreateRootSignature();
    void CreatePipelineState(ShaderCompiler* shaderCompiler);

    DirectXCommon* dxCommon_ = nullptr; // GameApplicationが所有。ここではdeleteしない。
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
