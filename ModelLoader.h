#pragma once
#include <string>
#include "ModelData.h"
#include "MaterialData.h"

class ModelLoader 
{
public:
    // Objファイルを読み込んでModelDataを返す関数
    static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

    // mtlファイルを読み込んでMaterialDataを返す関数
    static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
};