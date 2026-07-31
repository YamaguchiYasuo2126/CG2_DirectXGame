#include "DebugCamera.h"
#include "MatrixUtility.h"

void DebugCamera::Initialize() {
	// 射影行列（Projection行列）の初期化
	matProjection_ = MatrixUtility::MakePerspectiveFovMatrix(
		0.45f,
		1280.0f / 720.0f,
		0.1f,
		100.0f
	);

	// 初期変換行列の計算
	matView_ = MatrixUtility::MakeIdentity4x4();
	matViewProjection_ = MatrixUtility::MakeIdentity4x4();
}

void DebugCamera::Update(Input* input) {
	//入力によるカメラの移動や回転

	// 回転処理
	const float kRotateSpeed = 0.02f;

	if (input->PushKey(DIK_UP)) {
		rotation_.x -= kRotateSpeed; // X軸周りの角度を加算
	}
	if (input->PushKey(DIK_DOWN)) {
		rotation_.x += kRotateSpeed;
	}
	if (input->PushKey(DIK_LEFT)) {
		rotation_.y -= kRotateSpeed; // Y軸周りの角度を加算
	}
	if (input->PushKey(DIK_RIGHT)) {
		rotation_.y += kRotateSpeed;
	}

	// Z軸周りの回転（ロール）を追加
	if (input->PushKey(DIK_Z)) {
		rotation_.z -= kRotateSpeed;
	}
	if (input->PushKey(DIK_X)) {
		rotation_.z += kRotateSpeed;
	}

	// 現在の回転角度から回転行列を作成しておく（移動ベクトルの回転用）
	Matrix4x4 matRotX = MatrixUtility::MakeRotateXMatrix(rotation_.x);
	Matrix4x4 matRotY = MatrixUtility::MakeRotateYMatrix(rotation_.y);
	Matrix4x4 matRotZ = MatrixUtility::MakeRotateZMatrix(rotation_.z);
	Matrix4x4 matRot = MatrixUtility::Multiply(matRotX, MatrixUtility::Multiply(matRotY, matRotZ));

	// 移動処理
	const float kMoveSpeed = 0.5f;
	Vector3 move = { 0.0f, 0.0f, 0.0f };

	// ローカル（カメラ視点）での移動ベクトルを設定
	if (input->PushKey(DIK_W)) { move.z += kMoveSpeed; } // 前進
	if (input->PushKey(DIK_S)) { move.z -= kMoveSpeed; } // 後退
	if (input->PushKey(DIK_D)) { move.x += kMoveSpeed; } // 右移動
	if (input->PushKey(DIK_A)) { move.x -= kMoveSpeed; } // 左移動
	if (input->PushKey(DIK_E)) { move.y += kMoveSpeed; } // 上移動
	if (input->PushKey(DIK_Q)) { move.y -= kMoveSpeed; } // 下移動

	// 移動ベクトルをカメラの向きに合わせて回転させる
	// 方向ベクトル (move) と 回転行列 (matRot) の積を直接計算します
	Vector3 rotatedMove = {
		move.x * matRot.m[0][0] + move.y * matRot.m[1][0] + move.z * matRot.m[2][0],
		move.x * matRot.m[0][1] + move.y * matRot.m[1][1] + move.z * matRot.m[2][1],
		move.x * matRot.m[0][2] + move.y * matRot.m[1][2] + move.z * matRot.m[2][2]
	};

	// 回転後の移動ベクトル分だけ座標を加算
	translation_.x += rotatedMove.x;
	translation_.y += rotatedMove.y;
	translation_.z += rotatedMove.z;

	// ビュー行列の更新

	// 角度から回転行列を計算・座標から平行移動行列を計算し、ワールド行列を作る
	Matrix4x4 worldMatrix = MatrixUtility::MakeAffineMatrix(
		{ 1.0f, 1.0f, 1.0f },
		rotation_,
		translation_
	);

	// ワールド行列の逆行列をビュー行列に代入する
	matView_ = MatrixUtility::Inverse(worldMatrix);

	// オブジェクトに送るための合成行列も更新しておく
	matViewProjection_ = MatrixUtility::Multiply(matView_, matProjection_);
}