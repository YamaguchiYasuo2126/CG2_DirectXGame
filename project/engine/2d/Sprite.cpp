#include "Sprite.h"
#include "DirectXCommon.h"
#include "DirectXResource.h"
#include "MatrixUtility.h"
#include "SpriteRenderer.h"

#include <cassert>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void Sprite::Initialize(SpriteRenderer* spriteRenderer) {
    assert(spriteRenderer != nullptr && spriteRenderer->GetDirectXCommon() != nullptr);
    spriteRenderer_ = spriteRenderer;
    DirectXCommon* dxCommon = spriteRenderer_->GetDirectXCommon();


    // 頂点バッファの作成とデータ書き込み
    // Sprite用の頂点リソースを作る（4頂点分）
    vertexResourceSprite_ = DirectXResource::CreateBufferResource(dxCommon->GetDevice(), sizeof(VertexData) * 4);

    // 頂点バッファビューを作成する
    vertexBufferViewSprite_.BufferLocation = vertexResourceSprite_->GetGPUVirtualAddress();
    // VBVのサイズ指定4頂点分
    vertexBufferViewSprite_.SizeInBytes = sizeof(VertexData) * 4;
    vertexBufferViewSprite_.StrideInBytes = sizeof(VertexData);

    // データを書き込むためにMapする
    vertexResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite_));

    // 頂点データは重複をなくし、4つ（0, 1, 2, 3）だけ定義
    vertexDataSprite_[0].position = { 0.0f, 360.0f, 0.0f, 1.0f }; // 左下 (Index: 0)
    vertexDataSprite_[0].texcoord = { 0.0f, 1.0f };
    vertexDataSprite_[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };   // 左上 (Index: 1)
    vertexDataSprite_[1].texcoord = { 0.0f, 0.0f };
    vertexDataSprite_[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };// 右下 (Index: 2)
    vertexDataSprite_[2].texcoord = { 1.0f, 1.0f };
    vertexDataSprite_[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };  // 右上 (Index: 3)
    vertexDataSprite_[3].texcoord = { 1.0f, 0.0f };

    // Indexは6つ必要なので、要素数を6
    indexResourceSprite_ = DirectXResource::CreateBufferResource(dxCommon->GetDevice(), sizeof(uint32_t) * 6);

    // IndexBufferView (IBV) の設定
    indexBufferViewSprite_.BufferLocation = indexResourceSprite_->GetGPUVirtualAddress();
    indexBufferViewSprite_.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferViewSprite_.Format = DXGI_FORMAT_R32_UINT;

    // データを書き込むためにMapする
    indexResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite_));

    // インデックスデータの設定（三角形2枚分の頂点番号を指定）
    // 1枚目の三角形
    indexDataSprite_[0] = 0; // 左下
    indexDataSprite_[1] = 1; // 左上
    indexDataSprite_[2] = 2; // 右下
    // 2枚目の三角形
    indexDataSprite_[3] = 1; // 左上
    indexDataSprite_[4] = 3; // 右上
    indexDataSprite_[5] = 2; // 右下

    // TransformationMatrix（行列）バッファの作成
    // 行列1つ分のリソースを作る
    transformationMatrixResourceSprite_ = DirectXResource::CreateBufferResource(dxCommon->GetDevice(), sizeof(TransformationMatrix));

    // データを書き込むためにMapする
    transformationMatrixResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite_));

    // 初期値として単位行列を書き込んでおく
    transformationMatrixDataSprite_->WVP = MatrixUtility::MakeIdentity4x4();
    transformationMatrixDataSprite_->World = MatrixUtility::MakeIdentity4x4();

    // Sprite用のマテリアルリソースを作る
    materialResourceSprite_ = DirectXResource::CreateBufferResource(dxCommon->GetDevice(), sizeof(Material));

    // データを書き込むためにMapする
    materialResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite_));

    // 色は白を設定しておく
    materialDataSprite_->color = { 1.0f, 1.0f, 1.0f, 1.0f };

    // SpriteはLightingしないのでfalseを設定する
    materialDataSprite_->lightingMode = static_cast<int32_t>(LightingMode::None);

    // UVTransform行列を単位行列で初期化
    materialDataSprite_->uvTransform = MatrixUtility::MakeIdentity4x4();
}

void Sprite::Update() {
    DirectXCommon* dxCommon = spriteRenderer_->GetDirectXCommon();

    // WVPMatrixを作って定数バッファに書き込む

    // 2Dの状態を行列生成用の3D Transformへ変換する。Z座標とX/Y回転は使わない。
    const Transform transformSprite{
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, rotation_ },
        { position_.x, position_.y, 0.0f }
    };
    Matrix4x4 worldMatrixSprite = MatrixUtility::MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);

    // ビュー行列（2Dなので単位行列）
    Matrix4x4 viewMatrixSprite = MatrixUtility::MakeIdentity4x4();

    // プロジェクション行列（平行投影）
    float clientWidth = float(dxCommon->GetWidth());
    float clientHeight = float(dxCommon->GetHeight());
    Matrix4x4 projectionMatrixSprite = MatrixUtility::MakeOrthographicMatrix(0.0f, 0.0f, clientWidth, clientHeight, 0.0f, 100.0f);

    // WVP行列を合成 (World * View * Projection)
    Matrix4x4 worldViewProjectionMatrixSprite = MatrixUtility::Multiply(worldMatrixSprite, MatrixUtility::Multiply(viewMatrixSprite, projectionMatrixSprite));

    // バッファに書き込む
    transformationMatrixDataSprite_->WVP = worldViewProjectionMatrixSprite;
    transformationMatrixDataSprite_->World = worldMatrixSprite;

    // パラメータからUVTransform用の行列を生成する
    Matrix4x4 uvTransformMatrix = MatrixUtility::MakeScaleMatrix(uvTransformSprite_.scale);
    uvTransformMatrix = MatrixUtility::Multiply(uvTransformMatrix, MatrixUtility::MakeRotateZMatrix(uvTransformSprite_.rotate.z));
    uvTransformMatrix = MatrixUtility::Multiply(uvTransformMatrix, MatrixUtility::MakeTranslateMatrix(uvTransformSprite_.translate));

    materialDataSprite_->uvTransform = uvTransformMatrix;
}

void Sprite::Draw() {
    DirectXCommon* dxCommon = spriteRenderer_->GetDirectXCommon();
    // コマンドリストを取得
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    // Spriteの描画（変更が必要なものだけ設定する）
    // 頂点バッファ(VBV)をSprite用のものに差し替え
    commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite_);

    // インデックスバッファをセットするコマンド
    commandList->IASetIndexBuffer(&indexBufferViewSprite_);

    // マテリアルCBufferの場所を設定 (0番のRootParameter)
    commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite_->GetGPUVirtualAddress());

    // 1番のRootParameter（VertexShaderのWVP行列用）をSprite用のバッファに差し替え
    commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite_->GetGPUVirtualAddress());

    // 明示的にuvCheckerのSRVをセットする
    commandList->SetGraphicsRootDescriptorTable(2, dxCommon->GetSrvGpuHandle(1));

    // インデックスを使う
    // (描画するインデックス数, インスタンス数, インデックスの開始位置, 頂点の開始位置, インスタンスの開始位置)
    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::Finalize()
{
    vertexResourceSprite_.Reset();
    indexResourceSprite_.Reset();
    transformationMatrixResourceSprite_.Reset();
    materialResourceSprite_.Reset();
    vertexDataSprite_ = nullptr;
    indexDataSprite_ = nullptr;
    transformationMatrixDataSprite_ = nullptr;
    materialDataSprite_ = nullptr;
    spriteRenderer_ = nullptr;
}

void Sprite::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin("Settings");
    ImGui::PushID(this);
    if (ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
        // UIは一時コピーを編集し、公開setterを通してSpriteの状態を変更する。
        Vector2 position = GetPosition();
        if (ImGui::DragFloat2("Position (px)", &position.x, 1.0f)) {
            SetPosition(position);
        }
        float rotation = GetRotation();
        // ImGuiは度数で表示するが、受け渡すfloatはラジアンのまま。
        if (ImGui::SliderAngle("Rotation", &rotation, -180.0f, 180.0f)) {
            SetRotation(rotation);
        }
        if (ImGui::Button("Reset Transform")) {
            SetPosition({ 0.0f, 0.0f });
            SetRotation(0.0f);
        }
        ImGui::DragFloat2("UV Translate", &uvTransformSprite_.translate.x, 0.01f, -10.0f, 10.0f);
        ImGui::DragFloat2("UV Scale", &uvTransformSprite_.scale.x, 0.01f, -10.0f, 10.0f);
        ImGui::SliderAngle("UV Rotate", &uvTransformSprite_.rotate.z);
    }
    ImGui::PopID();
    ImGui::End();
#endif
}
