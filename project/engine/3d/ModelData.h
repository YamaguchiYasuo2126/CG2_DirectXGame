#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "VertexData.h"
#include "MaterialData.h"

// マテリアルが変わる箇所で分割された、1回の描画単位。
struct MeshData
{
    std::string name;
    std::vector<VertexData> vertices;
    uint32_t materialIndex = 0; // ModelData::materials内の参照先。
};

struct ModelData
{
    std::vector<MeshData> meshes;
    std::vector<MaterialData> materials;
};
