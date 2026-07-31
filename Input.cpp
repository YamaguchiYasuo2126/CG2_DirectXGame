#include "Input.h"
#include <cassert>

// pragmaはcpp側に書くとスッキリします
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

void Input::Initialize(HINSTANCE hInstance, HWND hwnd)
{
    HRESULT hr;

    // DirectInputオブジェクトの生成
    hr = DirectInput8Create(
        hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
        (void**)&directInput_, nullptr);
    assert(SUCCEEDED(hr));

    // キーボードデバイスの生成
    hr = directInput_->CreateDevice(GUID_SysKeyboard, &keyboard_, NULL);
    assert(SUCCEEDED(hr));

    // 入力データ形式のセット
    hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(hr));

    // 排他制御レベルのセット
    hr = keyboard_->SetCooperativeLevel(
        hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));
}

void Input::Update()
{
    // 今のキー状態を、前のキー状態として保存しておく
    memcpy(keyPre_, key_, sizeof(key_));

    // キーボード情報の取得開始
    keyboard_->Acquire();

    // 全キーの入力状態を取得する
    keyboard_->GetDeviceState(sizeof(key_), key_);
}

// 押しているか（今のフレームで 0x80 なら true）
bool Input::PushKey(uint8_t keyNum)
{
    if (key_[keyNum] == 0x80) {
        return true;
    }
    return false;
}

// 押した瞬間か（前のフレームで 0x00 かつ 今のフレームで 0x80 なら true）
bool Input::TriggerKey(uint8_t keyNum)
{
    if (keyPre_[keyNum] == 0x00 && key_[keyNum] == 0x80) {
        return true;
    }
    return false;
}