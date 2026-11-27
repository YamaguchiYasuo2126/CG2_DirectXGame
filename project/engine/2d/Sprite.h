#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>

#include "Matrix4x4.h"
#include "Vector2.h"
#include "Transform.h"
#include "VertexData.h"
#include "Material.h"
#include "TransformationMatrix.h"

// 共有描画設定はポインタとして参照するため、ここでは前方宣言だけでよい。
class SpriteRenderer;

// 2Dスプライトの初期化、更新、描画を担当
class Sprite {
public:
    // 初期化（頂点バッファや定数バッファの作成）
    void Initialize(SpriteRenderer* spriteRenderer);

    // 更新処理（行列の再計算など）
    void Update();

    // 頂点・インデックス・定数バッファを設定する。直前にSpriteRenderer::BeginDrawが必要。
    void Draw();

    void DrawImGui();

    // Map先への参照を解除し、スプライトが所有するバッファを解放する。
    void Finalize();

    // 左上を原点とする画面座標（ピクセル）。値はUpdateで描画用行列へ反映する。
    const Vector2& GetPosition() const { return position_; }
    void SetPosition(const Vector2& position) { position_ = position; }

    // 左上の頂点を中心としたZ軸回転。APIの単位はラジアン。
    float GetRotation() const { return rotation_; }
    void SetRotation(float rotation) { rotation_ = rotation; }

private:
    // 共有設定は借用。Spriteより先に初期化し、Spriteの終了後に破棄する。
    SpriteRenderer* spriteRenderer_ = nullptr;

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

    // Sprite自身が状態を所有する。呼び出し側に座標や角度の永続的な複製は不要。
    Vector2 position_{ 0.0f, 0.0f };
    float rotation_ = 0.0f;

    // UVTransform用の変数
    Transform uvTransformSprite_
    {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };
};
