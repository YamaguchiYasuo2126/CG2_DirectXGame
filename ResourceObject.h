#pragma once
#include <d3d12.h>

class ResourceObject
{

public:
    // コンストラクタ：生成時にポインタを受け取って保持する
    ResourceObject(ID3D12Resource* resource)
        : resource_(resource)
    {}

    // デストラクタ：このオブジェクトの寿命が尽きたときに自動でReleaseを呼ぶ
    ~ResourceObject() 
    {
        if (resource_) 
        {
            resource_->Release();
        }
    }

    // 生のポインタを取得するためのゲッター関数
    ID3D12Resource* Get() { return resource_; }

private:
    ID3D12Resource* resource_ = nullptr;

};

