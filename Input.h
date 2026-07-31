#pragma once
#include <Windows.h>

#define DIRECTINPUT_VERSION 0x0800 // ヘッダより上に書く
#include <dinput.h>
#include <wrl.h>
#include <stdint.h>

class Input
{
public:
    // 初期化（ウィンドウ生成時に得られる情報をもらう）
    void Initialize(HINSTANCE hInstance, HWND hwnd);

    // 毎フレーム行う更新処理（キーボードの入力状態を取得するなど）
    void Update();

    bool PushKey(uint8_t keyNum);    // 押しているか
    bool TriggerKey(uint8_t keyNum); // 押した瞬間か

private:
    // スライドにあったポインタたちをComPtrで管理
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;

    // 全キーの入力状態を保存する配列（今フレームと前フレーム）
    BYTE key_[256] = {};
    BYTE keyPre_[256] = {};
};