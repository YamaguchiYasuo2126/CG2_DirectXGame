#pragma once
#include <string>
#include "ModelData.h"
#include "MaterialData.h"

class ModelLoader 
{
public:
    // directoryPath内のOBJ/MTLを読む。左手系へ変換し、多角形の面を三角形へ分割する。
    static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

    // mtlファイルを読み込んでMaterialDataを返す関数
    static std::vector<MaterialData> LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
};
