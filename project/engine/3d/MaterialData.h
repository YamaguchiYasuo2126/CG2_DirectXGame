#pragma once
#include <string>

// MTLから読み込むCPU側データ。GPUリソースはObject3d/Particle側で作成する。
struct MaterialData {
    std::string name;
    std::string textureFilePath;
};
