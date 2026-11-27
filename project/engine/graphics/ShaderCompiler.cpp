#include "ShaderCompiler.h"

#include <cassert>
#include <filesystem>
#include <format>

#include "ConvertString.h"
#include "Logger.h"

#pragma comment(lib, "dxcompiler.lib")

void ShaderCompiler::Initialize(Logger* logger) {
	logger_ = logger;

	// DXCのUtility、Compiler、IncludeHandlerをまとめて準備します。
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr));
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr));
}

void ShaderCompiler::Finalize() {
	includeHandler_.Reset();
	dxcCompiler_.Reset();
	dxcUtils_.Reset();
	logger_ = nullptr;
}

Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::Compile(const std::wstring& filePath, const wchar_t* profile) {
	// 開発時はプロジェクト直下、配布時は projectFolder 内に HLSL を置く。
	std::filesystem::path shaderPath(filePath);
	if (!std::filesystem::exists(shaderPath)) {
		const std::filesystem::path packagedPath = std::filesystem::path(L"projectFolder") / shaderPath;
		if (std::filesystem::exists(packagedPath)) { shaderPath = packagedPath; }
	}
	const std::wstring sourcePath = shaderPath.wstring();
	const std::wstring includeDirectory = shaderPath.has_parent_path()
		? shaderPath.parent_path().wstring() : L".";
	logger_->Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", sourcePath, profile)));

	// HLSLファイルを読み込み、DXCへ渡すためのバッファに変換します。
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource;
	HRESULT hr = dxcUtils_->LoadFile(sourcePath.c_str(), nullptr, &shaderSource);
	if (FAILED(hr)) {
		logger_->Log(ConvertString(std::format(L"Shader file not found: {} (HRESULT 0x{:08X})\n",
			sourcePath, static_cast<unsigned int>(hr))));
	}
	assert(SUCCEEDED(hr));

	DxcBuffer shaderSourceBuffer{};
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	// EntryPointはmain、Profileは呼び出し側から指定されたVS/PS用のものを使います。
	LPCWSTR arguments[] = {
		sourcePath.c_str(),
		L"-E", L"main",
		L"-T", profile,
		L"-I", includeDirectory.c_str(),
		L"-Zi", L"-Qembed_debug",
		L"-Od",
		L"-Zpr",
	};

	// DXCでコンパイルし、エラーがあればログへ出して停止します。
	Microsoft::WRL::ComPtr<IDxcResult> shaderResult;
	hr = dxcCompiler_->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler_.Get(),
		IID_PPV_ARGS(&shaderResult));
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		logger_->Log(shaderError->GetStringPointer());
		assert(false);
	}

	// コンパイル済みShaderバイナリを取り出して呼び出し側へ返します。
	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));

	logger_->Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
	return shaderBlob;
}
