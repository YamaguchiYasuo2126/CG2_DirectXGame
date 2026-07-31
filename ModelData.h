#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "VertexData.h"
#include "MaterialData.h"

struct MeshData {
    std::string name;
    std::vector<VertexData> vertices;
    uint32_t materialIndex = 0;
};

struct ModelData {
    std::vector<MeshData> meshes;
    std::vector<MaterialData> materials;
};
