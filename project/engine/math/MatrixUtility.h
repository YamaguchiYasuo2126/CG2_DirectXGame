#pragma once
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"
/// <summary>
/// 行列用ユーティリティクラス
/// </summary>
class MatrixUtility {
public:
	// 行列の積
	static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

	// 単位行列の作成
	static Matrix4x4 MakeIdentity4x4();

	// 逆行列
	static Matrix4x4 Inverse(const Matrix4x4& m);

	// 平行移動行列の作成
	static Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

	// 拡大縮小行列の作成
	static Matrix4x4 MakeScaleMatrix(const Vector3& scale);

	// X軸回転行列
	static Matrix4x4 MakeRotateXMatrix(float radian);

	// Y軸回転行列
	static Matrix4x4 MakeRotateYMatrix(float radian);

	// Z軸回転行列
	static Matrix4x4 MakeRotateZMatrix(float radian);

	// 3次元アフィン変換行列の作成（Scale -> Rotate -> Translate）
	static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	// 透視投影行列
	static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	// 正射影行列
	static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
};

