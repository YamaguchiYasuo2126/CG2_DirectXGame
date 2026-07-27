#include "ModelLoader.h"
#include <fstream>
#include <sstream>
#include <cassert>

ModelData ModelLoader::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
    // 中で必要となる変数の宣言
    ModelData modelData; // 構築するModelData
    std::vector<Vector4> positions; // 位置
    std::vector<Vector3> normals; // 法線
    std::vector<Vector2> texcoords; // テクスチャ座標
    std::string line; // ファイルから読んだ1行を格納するもの

    // ファイルを開く
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    assert(file.is_open()); // とりあえず開けなかったら止める

    // 実際にファイルを読み、ModelDataを構築していく
    while (std::getline(file, line))
    {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier; // 先頭の識別子を読む

        // identifierに応じた処理
        if (identifier == "v") 
        {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            // 右手系から左手系への変換のため、X座標を反転する
            position.x *= -1.0f;
            position.w = 1.0f;
            positions.push_back(position);
        }
        else if (identifier == "vt") 
        {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            // DirectXに合わせてV方向を反転させる
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);
        }
        else if (identifier == "vn")
        {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            // 法線のX方向も同様に反転する
            normal.x *= -1.0f;
            normals.push_back(normal);
        }
        else if (identifier == "f") 
        {
            std::vector<VertexData> faceVertices; // 一時的に面の頂点を保存する配列
            std::string vertexDefinition;

            // 行の残りにある頂点定義をすべて読み込む（四角形ポリゴンなどに対応）
            while (s >> vertexDefinition) 
            {
                std::istringstream v(vertexDefinition);
                uint32_t elementIndices[3] = { 0, 0, 0 };
                for (int32_t element = 0; element < 3; ++element)
                {
                    std::string index;
                    std::getline(v, index, '/');
                    if (!index.empty()) {
                        elementIndices[element] = std::stoi(index);
                    }
                }

                Vector4 position = positions[elementIndices[0] - 1];
                Vector2 texcoord = texcoords[elementIndices[1] - 1];
                Vector3 normal = normals[elementIndices[2] - 1];

                faceVertices.push_back({ position, texcoord, normal });
            }

            // 読み込んだ頂点を三角形に分割して登録（右手系から左手系への変換のため逆順に）
            for (size_t i = 1; i < faceVertices.size() - 1; ++i) {
                modelData.vertices.push_back(faceVertices[i + 1]);
                modelData.vertices.push_back(faceVertices[i]);
                modelData.vertices.push_back(faceVertices[0]);
            }
        }
        else if (identifier == "mtllib")
        {
            // materialTemplateLibraryファイルの名前を取得する
            std::string materialFilename;
            s >> materialFilename;
            // 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
            modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
        }

    }

    // ModelDataを返す
    return modelData;
}

MaterialData ModelLoader::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename)
{
    // 必要な変数の宣言を行い、ファイルを開く
    MaterialData materialData; // 構築するMaterialData
    std::string line; // ファイルから読んだ1行を格納するもの
    std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
    assert(file.is_open()); // とりあえず開けなかったら止める

    // 実際にファイルを読み、MaterialDataを構築していく
    while (std::getline(file, line))
    {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        // identifierに応じた処理
        if (identifier == "map_Kd")
        {
            std::string textureFilename;
            s >> textureFilename;
            // 連結してファイルパスにする
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }

    return materialData;
}