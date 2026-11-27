#pragma once
#include <Windows.h>
#include "WinApp.h"

#define DIRECTINPUT_VERSION 0x0800 // dinput.hより上に書く
#include <dinput.h>
#include <wrl.h>
#include <stdint.h>


class Input
{
public:
    // namespace省略
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    // 初期化（ウィンドウ生成時に得られる情報をもらう）
    void Initialize(WinApp* winApp);

    // 毎フレーム行う更新処理（キーボードの入力状態を取得するなど）
    void Update();

    // キーボードの取得状態を解除してからDirectInputを解放する。
    void Finalize();

    /// <summary>
    /// キーの押下をチェック
    /// </summary>
    /// <param name="keyNum"></param>
    /// <returns>押しているか</returns>
    bool PushKey(uint8_t keyNum);

    /// <summary>
    /// キーのトリガーをチェック
    /// </summary>
    /// <param name="keyNum"></param>
    /// <returns>押した瞬間か</returns>
    bool TriggerKey(uint8_t keyNum);

private:
    // ウィンドウ情報は借用。InputのFinalizeが終わるまでWinAppを生存させる。
    WinApp* winApp_ = nullptr;

    // DirectInputのポインタをComPtrで管理
    ComPtr<IDirectInput8> directInput_;
    // キーボードデバイスのポインタをComPtrで管理
    ComPtr<IDirectInputDevice8> keyboard_;

    // DIK_*を添字に使う。前フレームとの比較で押した瞬間を検出する。
    BYTE key_[256] = {};
    BYTE keyPre_[256] = {};
};
