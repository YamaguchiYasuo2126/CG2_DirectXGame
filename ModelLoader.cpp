#include "ModelLoader.h"

#include <cassert>
#include <fstream>
#include <sstream>

namespace {
uint32_t FindMaterialIndex(const std::vector<MaterialData>& materials, const std::string& name) {
    for (uint32_t index = 0; index < materials.size(); ++index) {
        if (materials[index].name == name) {
            return index;
        }
    }
    return 0;
}
}

ModelData ModelLoader::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    ModelData modelData;
    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;

    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    std::string currentMeshName = "Default";
    uint32_t currentMaterialIndex = 0;
    MeshData* currentMesh = nullptr;

    auto beginMesh = [&]() -> MeshData& {
        if (currentMesh == nullptr) {
            modelData.meshes.push_back({ currentMeshName, {}, currentMaterialIndex });
            currentMesh = &modelData.meshes.back();
        }
        return *currentMesh;
    };

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string identifier;
        stream >> identifier;

        if (identifier == "v") {
            Vector4 position{};
            stream >> position.x >> position.y >> position.z;
            position.x *= -1.0f;
            position.w = 1.0f;
            positions.push_back(position);
        }
        else if (identifier == "vt") {
            Vector2 texcoord{};
            stream >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);
        }
        else if (identifier == "vn") {
            Vector3 normal{};
            stream >> normal.x >> normal.y >> normal.z;
            normal.x *= -1.0f;
            normals.push_back(normal);
        }
        else if (identifier == "mtllib") {
            std::string materialFilename;
            stream >> materialFilename;
            modelData.materials = LoadMaterialTemplateFile(directoryPath, materialFilename);
        }
        else if (identifier == "o" || identifier == "g") {
            stream >> currentMeshName;
            if (currentMeshName.empty()) {
                currentMeshName = "Unnamed Mesh";
            }
            currentMesh = nullptr;
        }
        else if (identifier == "usemtl") {
            std::string materialName;
            stream >> materialName;
            currentMaterialIndex = FindMaterialIndex(modelData.materials, materialName);

            // A material change needs a new draw call, so split it into a submesh.
            if (currentMesh != nullptr && !currentMesh->vertices.empty()) {
                currentMesh = nullptr;
            }
        }
        else if (identifier == "f") {
            std::vector<VertexData> faceVertices;
            std::string vertexDefinition;

            while (stream >> vertexDefinition) {
                std::istringstream vertexStream(vertexDefinition);
                uint32_t elementIndices[3] = {};
                for (int32_t element = 0; element < 3; ++element) {
                    std::string index;
                    std::getline(vertexStream, index, '/');
                    if (!index.empty()) {
                        elementIndices[element] = static_cast<uint32_t>(std::stoul(index));
                    }
                }

                const Vector4 position = positions[elementIndices[0] - 1];
                const Vector2 texcoord = elementIndices[1] == 0 ? Vector2{} : texcoords[elementIndices[1] - 1];
                const Vector3 normal = elementIndices[2] == 0 ? Vector3{ 0.0f, 1.0f, 0.0f } : normals[elementIndices[2] - 1];
                faceVertices.push_back({ position, texcoord, normal });
            }

            MeshData& mesh = beginMesh();
            for (size_t index = 1; index + 1 < faceVertices.size(); ++index) {
                mesh.vertices.push_back(faceVertices[index + 1]);
                mesh.vertices.push_back(faceVertices[index]);
                mesh.vertices.push_back(faceVertices[0]);
            }
        }
    }

    if (modelData.materials.empty()) {
        modelData.materials.push_back({ "Default", directoryPath + "/uvChecker.png" });
    }
    return modelData;
}

std::vector<MaterialData> ModelLoader::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
    std::vector<MaterialData> materials;
    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    MaterialData* currentMaterial = nullptr;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream stream(line);
        std::string identifier;
        stream >> identifier;

        if (identifier == "newmtl") {
            std::string name;
            stream >> name;
            materials.push_back({ name, directoryPath + "/uvChecker.png" });
            currentMaterial = &materials.back();
        }
        else if (identifier == "map_Kd" && currentMaterial != nullptr) {
            std::string textureFilename;
            stream >> textureFilename;
            currentMaterial->textureFilePath = directoryPath + "/" + textureFilename;
        }
    }
    return materials;
}
