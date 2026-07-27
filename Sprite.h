#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>

#include "Matrix4x4.h"
#include "Transform.h"
#include "VertexData.h"
#include "Material.h"
#include "TransformationMatrix.h"

// DirectXCommonクラスの前方宣言（ポインタとして受け取るため）
class DirectXCommon;

// 2Dスプライトの初期化、更新、描画を担当
class Sprite {
public:
    // 初期化（頂点バッファや定数バッファの作成）
    void Initialize(DirectXCommon* dxCommon);

    // 更新処理（行列の再計算など）
    void Update();

    // 描画処理（VBVとTransformationMatrixのみセットして描画）
    void Draw();

    void DrawImGui();

    void Finalize();

private:
    // 描画コマンド発行やデバイス取得に使う共通クラス
    DirectXCommon* dxCommon_ = nullptr;

    // Sprite用のマテリアルリソースとデータポインタ
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite_;
    Material* materialDataSprite_ = nullptr;

    // --- リソース関連 ---
    // Sprite用の頂点リソース（三角形2枚分）
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite_;
    // 頂点バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite_{};
    // 頂点データ書き込み用ポインタ（Map用）
    VertexData* vertexDataSprite_ = nullptr;

    // インデックスバッファ用の変数
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite_;
    D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite_{};
    uint32_t* indexDataSprite_ = nullptr;

    // Sprite用のTransformationMatrix（WVP行列）リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite_;
    // 行列データ書き込み用ポインタ（Map用）
    TransformationMatrix* transformationMatrixDataSprite_ = nullptr;

    // --- Transform関連 ---
    // CPUで動かす用のTransform（初期位置、回転、スケール）
    Transform transformSprite_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
};