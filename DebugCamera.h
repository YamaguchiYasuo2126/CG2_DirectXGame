#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"
#include "Input.h"

/// <summary>
/// デバッグカメラ
/// </summary>
class DebugCamera {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update(Input* input);

	// ゲッター関数
	const Matrix4x4& GetMatView() const { return matView_; }
	const Matrix4x4& GetMatProjection() const { return matProjection_; }
	Matrix4x4 GetViewProjectionMatrix() const { return matViewProjection_; }

private:
	// X, Y, Z軸周りのローカル回転角
	Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
	// ローカル座標
	Vector3 translation_ = { 0.0f, 0.0f, -50.0f };

	// ビュー行列
	Matrix4x4 matView_;
	// 射影行列
	Matrix4x4 matProjection_;
	// ビュープロジェクション行列（合成用）
	Matrix4x4 matViewProjection_;
};

