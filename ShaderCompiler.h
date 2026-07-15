#pragma once

#include <Windows.h>

#include <dxcapi.h>
#include <string>
#include <wrl.h>

class Logger;

// HLSLファイルをDXCでコンパイルし、Pipeline作成に使うShaderBlobを返します。
class ShaderCompiler {
public:
	void Initialize(Logger* logger);
	void Finalize();
	Microsoft::WRL::ComPtr<IDxcBlob> Compile(const std::wstring& filePath, const wchar_t* profile);

private:
	// DXC関連のCOMオブジェクトです。Compileで使い回します。
	Logger* logger_ = nullptr;
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};
