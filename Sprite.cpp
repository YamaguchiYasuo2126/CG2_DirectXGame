#include "Sprite.h"
#include "DirectXCommon.h"
#include "DirectXResource.h"
#include "MatrixUtility.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void Sprite::Initialize(DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;


    // 頂点バッファの作成とデータ書き込み
    // Sprite用の頂点リソースを作る（6頂点分）
    vertexResourceSprite_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * 6);

    // 頂点バッファビューを作成する
    vertexBufferViewSprite_.BufferLocation = vertexResourceSprite_->GetGPUVirtualAddress();
    vertexBufferViewSprite_.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferViewSprite_.StrideInBytes = sizeof(VertexData);

    // データを書き込むためにMapする
    vertexResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite_));

    // 1枚目の三角形（左下、左上、右下）
    vertexDataSprite_[0].position = { 0.0f, 360.0f, 0.0f, 1.0f }; // 左下
    vertexDataSprite_[0].texcoord = { 0.0f, 1.0f };
    vertexDataSprite_[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };   // 左上
    vertexDataSprite_[1].texcoord = { 0.0f, 0.0f };
    vertexDataSprite_[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };// 右下
    vertexDataSprite_[2].texcoord = { 1.0f, 1.0f };

    // 2枚目の三角形（左上、右上、右下）
    vertexDataSprite_[3].position = { 0.0f, 0.0f, 0.0f, 1.0f };   // 左上
    vertexDataSprite_[3].texcoord = { 0.0f, 0.0f };
    vertexDataSprite_[4].position = { 640.0f, 0.0f, 0.0f, 1.0f }; // 右上
    vertexDataSprite_[4].texcoord = { 1.0f, 0.0f };
    vertexDataSprite_[5].position = { 640.0f, 360.0f, 0.0f, 1.0f };// 右下
    vertexDataSprite_[5].texcoord = { 1.0f, 1.0f };

    // TransformationMatrix（行列）バッファの作成
    // 行列1つ分のリソースを作る
    transformationMatrixResourceSprite_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Matrix4x4));

    // データを書き込むためにMapする
    transformationMatrixResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite_));

    // 初期値として単位行列を書き込んでおく
    *transformationMatrixDataSprite_ = MatrixUtility::MakeIdentity4x4();
}

void Sprite::Update() {

    // WVPMatrixを作って定数バッファに書き込む

    //  ワールド行列（SpriteのTransformから作成）
    Matrix4x4 worldMatrixSprite = MatrixUtility::MakeAffineMatrix(transformSprite_.scale, transformSprite_.rotate, transformSprite_.translate);

    // ビュー行列（2Dなので単位行列でOK）
    Matrix4x4 viewMatrixSprite = MatrixUtility::MakeIdentity4x4();

    // プロジェクション行列（平行投影）
    // スクリーンショットの kClientWidth / kClientHeight は dxCommon_ から取得します
    float clientWidth = float(dxCommon_->GetWidth());
    float clientHeight = float(dxCommon_->GetHeight());
    Matrix4x4 projectionMatrixSprite = MatrixUtility::MakeOrthographicMatrix(0.0f, 0.0f, clientWidth, clientHeight, 0.0f, 100.0f);

    // WVP行列を合成 (World * View * Projection)
    Matrix4x4 worldViewProjectionMatrixSprite = MatrixUtility::Multiply(worldMatrixSprite, MatrixUtility::Multiply(viewMatrixSprite, projectionMatrixSprite));

    // バッファに書き込む
    *transformationMatrixDataSprite_ = worldViewProjectionMatrixSprite;
}

void Sprite::Draw() {
    // コマンドリストを取得
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // Spriteの描画（変更が必要なものだけ設定する）
    // 頂点バッファ(VBV)をSprite用のものに差し替え
    commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite_);

    // 1番のRootParameter（VertexShaderのWVP行列用）をSprite用のバッファに差し替え
    // ※ 0番(マテリアル)と2番(テクスチャ)は3D描画の時の設定がそのまま残っているので設定不要です
    commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite_->GetGPUVirtualAddress());

    // 描画（ドローコール）
    commandList->DrawInstanced(6, 1, 0, 0);
}

void Sprite::Finalize() 
{
    vertexResourceSprite_.Reset();
    transformationMatrixResourceSprite_.Reset();
}

void Sprite::DrawImGui() 
{
#ifdef USE_IMGUI
    ImGui::Begin("Settings");

    // transformSprite_ の translate (平行移動) の X, Y, Z をいじれるようにする
    // ドラッグしたときの変化量を 1.0f に設定
    ImGui::DragFloat3("translateSprite", &transformSprite_.translate.x, 1.0f);

    ImGui::End();
#endif
}