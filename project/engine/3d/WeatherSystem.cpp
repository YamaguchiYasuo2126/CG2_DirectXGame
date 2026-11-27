#include "WeatherSystem.h"

#include <cassert>
#include <cmath>
#include <cstring>

#include "DirectXCommon.h"
#include "DirectXResource.h"
#include "MatrixUtility.h"
#include "ShaderCompiler.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr float kSpawnHalfWidth = 6.0f;
constexpr float kSpawnNear = 2.0f;
constexpr float kSpawnFar = 18.0f;
}

void WeatherSystem::Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler)
{
	dxCommon_ = dxCommon;
	ID3D12Device* device = dxCommon_->GetDevice();

	const std::array<WeatherVertex, 6> vertices = {
		WeatherVertex{ { -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
		WeatherVertex{ {  0.5f,  0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
		WeatherVertex{ { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
		WeatherVertex{ {  0.5f,  0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
		WeatherVertex{ {  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
		WeatherVertex{ { -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
	};
	vertexResource_ = DirectXResource::CreateBufferResource(device, sizeof(vertices));
	void* vertexData = nullptr;
	HRESULT hr = vertexResource_->Map(0, nullptr, &vertexData);
	assert(SUCCEEDED(hr));
	std::memcpy(vertexData, vertices.data(), sizeof(vertices));
	vertexResource_->Unmap(0, nullptr);
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(vertices));
	vertexBufferView_.StrideInBytes = sizeof(WeatherVertex);

	instanceResource_ = DirectXResource::CreateBufferResource(device, sizeof(WeatherInstance) * kMaxParticles);
	hr = instanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));
	assert(SUCCEEDED(hr));
	// Root CBVのGPUアドレスを256バイト境界のバッファに置く。
	sandHazeResource_ = DirectXResource::CreateBufferResource(device, 256);
	hr = sandHazeResource_->Map(0, nullptr, reinterpret_cast<void**>(&sandHazeData_));
	assert(SUCCEEDED(hr));
	*sandHazeData_ = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = kMaxParticles;
	srvDesc.Buffer.StructureByteStride = sizeof(WeatherInstance);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	const uint32_t srvIndex = dxCommon_->AllocateSrvIndex();
	device->CreateShaderResourceView(instanceResource_.Get(), &srvDesc, dxCommon_->GetSrvCpuHandle(srvIndex));
	instanceSrvHandle_ = dxCommon_->GetSrvGpuHandle(srvIndex);

	D3D12_DESCRIPTOR_RANGE instanceRange{};
	instanceRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	instanceRange.NumDescriptors = 1;
	instanceRange.BaseShaderRegister = 0;
	instanceRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	D3D12_ROOT_PARAMETER rootParameters[2]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &instanceRange;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0;
	D3D12_ROOT_SIGNATURE_DESC rootDesc{};
	rootDesc.NumParameters = _countof(rootParameters);
	rootDesc.pParameters = rootParameters;
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	vertexShaderBlob_ = shaderCompiler->Compile(L"resources/shaders/Weather.VS.hlsl", L"vs_6_0");
	pixelShaderBlob_ = shaderCompiler->Compile(L"resources/shaders/Weather.PS.hlsl", L"ps_6_0");
	snowPixelShaderBlob_ = shaderCompiler->Compile(L"resources/shaders/Weather.Snow.PS.hlsl", L"ps_6_0");
	sandPixelShaderBlob_ = shaderCompiler->Compile(L"resources/shaders/Weather.Sand.PS.hlsl", L"ps_6_0");
	sandHazeVertexShaderBlob_ = shaderCompiler->Compile(L"resources/shaders/Weather.SandHaze.VS.hlsl", L"vs_6_0");
	sandHazePixelShaderBlob_ = shaderCompiler->Compile(L"resources/shaders/Weather.SandHaze.PS.hlsl", L"ps_6_0");
	D3D12_INPUT_ELEMENT_DESC inputElements[2]{};
	inputElements[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
		D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElements[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
		D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.InputLayout = { inputElements, _countof(inputElements) };
	pipelineDesc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };
	pipelineDesc.PS = { pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize() };
	pipelineDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	pipelineDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	pipelineDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	pipelineDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	pipelineDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	pipelineDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	pipelineDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	pipelineDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pipelineDesc.RasterizerState.DepthClipEnable = TRUE;
	pipelineDesc.DepthStencilState.DepthEnable = TRUE;
	pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = dxCommon_->GetRenderTargetFormat();
	pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineDesc.SampleDesc.Count = 1;
	hr = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&rainPipelineState_));
	assert(SUCCEEDED(hr));
	pipelineDesc.PS = { snowPixelShaderBlob_->GetBufferPointer(), snowPixelShaderBlob_->GetBufferSize() };
	hr = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&snowPipelineState_));
	assert(SUCCEEDED(hr));
	pipelineDesc.PS = { sandPixelShaderBlob_->GetBufferPointer(), sandPixelShaderBlob_->GetBufferSize() };
	hr = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&sandPipelineState_));
	assert(SUCCEEDED(hr));
	// 霞は深度に関係なく画面全体に重ねる。ImGuiはこの後に描く。
	pipelineDesc.InputLayout = { nullptr, 0 };
	pipelineDesc.VS = { sandHazeVertexShaderBlob_->GetBufferPointer(), sandHazeVertexShaderBlob_->GetBufferSize() };
	pipelineDesc.PS = { sandHazePixelShaderBlob_->GetBufferPointer(), sandHazePixelShaderBlob_->GetBufferSize() };
	pipelineDesc.DepthStencilState.DepthEnable = FALSE;
	hr = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&sandHazePipelineState_));
	assert(SUCCEEDED(hr));
}

void WeatherSystem::Update(float deltaTime, const Vector3& cameraPosition,
	const Matrix4x4& cameraWorld, const Matrix4x4& viewProjection)
{
	assert(deltaTime >= 0.0f);
	if (mode_ == Mode::Off)
	{
		particles_.fill(WeatherParticle{});
		spawnCredit_ = 0.0f;
		instanceCount_ = 0;
		return;
	}
	if (mode_ == Mode::Sandstorm)
	{
		sandTime_ += deltaTime;
		sandHazeData_->time = sandTime_;
		sandHazeData_->opacity = sandVeilOpacity_;
		sandHazeData_->flowX = std::cos(sandDirection_) * sandSpeed_ + windX_;
		sandHazeData_->flowZ = std::sin(sandDirection_) * sandSpeed_ + windZ_;
	}
	const float sandVelocityX = std::cos(sandDirection_) * sandSpeed_ + windX_;
	const float sandVelocityZ = std::sin(sandDirection_) * sandSpeed_ + windZ_;

	for (WeatherParticle& particle : particles_)
	{
		if (!particle.alive) { continue; }
		particle.age += deltaTime;
		if (mode_ == Mode::Sandstorm)
		{
			particle.position.x += (sandVelocityX + 0.6f * std::sin(particle.age * 9.0f + particle.flutterPhase)) * deltaTime;
			particle.position.z += (sandVelocityZ + 0.3f * std::cos(particle.age * 7.0f + particle.flutterPhase)) * deltaTime;
			particle.position.y += (0.4f * std::sin(particle.age * 5.0f + particle.flutterPhase) - 0.15f) * deltaTime;
			if (particle.age >= 2.0f) { particle.alive = false; }
		}
		else if (mode_ == Mode::Snow)
		{
			particle.position.x += (windX_ + 0.35f * std::sin(particle.age * 2.1f + particle.flutterPhase)) * deltaTime;
			particle.position.z += (windZ_ + 0.15f * std::cos(particle.age * 1.7f + particle.flutterPhase)) * deltaTime;
			particle.position.y -= snowFallSpeed_ * particle.speedScale * deltaTime;
		}
		else
		{
			particle.position.x += windX_ * deltaTime;
			particle.position.z += windZ_ * deltaTime;
			particle.position.y -= rainFallSpeed_ * particle.speedScale * deltaTime;
		}
		if (particle.position.y < cameraPosition.y - 3.0f ||
			std::abs(particle.position.x - cameraPosition.x) > kSpawnHalfWidth + 3.0f ||
			particle.position.z < cameraPosition.z - 3.0f ||
			particle.position.z > cameraPosition.z + kSpawnFar + 3.0f)
		{
			particle.alive = false;
		}
	}

	const float emissionRate = mode_ == Mode::Rain ? rainPerSecond_ :
		(mode_ == Mode::Snow ? snowPerSecond_ : sandPerSecond_);
	spawnCredit_ += emissionRate * deltaTime;
	const uint32_t toSpawn = static_cast<uint32_t>(spawnCredit_);
	spawnCredit_ -= static_cast<float>(toSpawn);
	std::uniform_real_distribution<float> xDistribution(
		mode_ == Mode::Sandstorm ? -4.5f : -kSpawnHalfWidth,
		mode_ == Mode::Sandstorm ? 4.5f : kSpawnHalfWidth);
	std::uniform_real_distribution<float> yDistribution(
		mode_ == Mode::Rain ? 2.0f : (mode_ == Mode::Snow ? -2.0f : -2.4f),
		mode_ == Mode::Rain ? 6.0f : (mode_ == Mode::Snow ? 3.0f : 2.7f));
	std::uniform_real_distribution<float> zDistribution(
		mode_ == Mode::Sandstorm ? 1.2f : kSpawnNear,
		mode_ == Mode::Sandstorm ? 12.0f : kSpawnFar);
	std::uniform_real_distribution<float> speedDistribution(0.8f, 1.2f);
	std::uniform_real_distribution<float> alphaDistribution(
		mode_ == Mode::Rain ? 0.25f : 0.6f,
		mode_ == Mode::Rain ? 0.45f : 0.9f);
	std::uniform_real_distribution<float> snowSizeDistribution(0.05f, 0.10f);
	std::uniform_real_distribution<float> sandSizeDistribution(0.07f, 0.16f);
	std::uniform_real_distribution<float> cloudSizeDistribution(1.4f, 3.2f);
	std::uniform_real_distribution<float> sandAlphaDistribution(0.55f, 0.9f);
	std::uniform_real_distribution<float> cloudAlphaDistribution(0.08f, 0.16f);
	std::uniform_int_distribution<int> cloudChance(0, 31);
	std::uniform_real_distribution<float> phaseDistribution(0.0f, 6.2831853f);
	uint32_t emitted = 0;
	uint32_t scanned = 0;
	for (; scanned < kMaxParticles && emitted < toSpawn; ++scanned)
	{
		WeatherParticle& particle = particles_[(spawnCursor_ + scanned) % kMaxParticles];
		if (particle.alive) { continue; }
		particle.position = { cameraPosition.x + xDistribution(randomEngine_),
			cameraPosition.y + yDistribution(randomEngine_),
			cameraPosition.z + zDistribution(randomEngine_) };
		particle.speedScale = speedDistribution(randomEngine_);
		particle.dustCloud = mode_ == Mode::Sandstorm && cloudChance(randomEngine_) == 0;
		particle.size = particle.dustCloud ? cloudSizeDistribution(randomEngine_) :
			(mode_ == Mode::Snow ? snowSizeDistribution(randomEngine_) :
				(mode_ == Mode::Sandstorm ? sandSizeDistribution(randomEngine_) : 1.0f));
		particle.alpha = particle.dustCloud ? cloudAlphaDistribution(randomEngine_) :
			(mode_ == Mode::Sandstorm ? sandAlphaDistribution(randomEngine_) : alphaDistribution(randomEngine_));
		particle.age = 0.0f;
		particle.flutterPhase = phaseDistribution(randomEngine_);
		particle.alive = true;
		++emitted;
	}
	spawnCursor_ = (spawnCursor_ + scanned) % kMaxParticles;

	Matrix4x4 billboardRotation = cameraWorld;
	billboardRotation.m[3][0] = 0.0f;
	billboardRotation.m[3][1] = 0.0f;
	billboardRotation.m[3][2] = 0.0f;
	instanceCount_ = 0;
	for (const WeatherParticle& particle : particles_)
	{
		if (!particle.alive) { continue; }
		WeatherInstance& instance = instanceData_[instanceCount_++];
		Matrix4x4 world;
		if (mode_ != Mode::Rain)
		{
			world = MatrixUtility::Multiply(
				MatrixUtility::MakeScaleMatrix({ particle.size, particle.size, 1.0f }),
				billboardRotation);
			world.m[3][0] = particle.position.x;
			world.m[3][1] = particle.position.y;
			world.m[3][2] = particle.position.z;
			if (mode_ == Mode::Snow)
			{
				instance.color = { 0.92f, 0.97f, 1.0f, particle.alpha };
			}
			else
			{
				instance.color = particle.dustCloud
					? Vector4{ 0.68f, 0.52f, 0.33f, particle.alpha }
					: Vector4{ 0.90f, 0.70f, 0.43f, particle.alpha };
			}
		}
		else
		{
			world = MatrixUtility::MakeAffineMatrix(
				{ 0.012f, 0.14f, 1.0f }, { 0.0f, 0.0f, 0.0f }, particle.position);
			instance.color = { 0.65f, 0.8f, 1.0f, particle.alpha };
		}
		instance.WVP = MatrixUtility::Multiply(world, viewProjection);
	}
	assert(instanceCount_ <= kMaxParticles);
}

void WeatherSystem::Draw() const
{
	if (instanceCount_ == 0 && mode_ != Mode::Sandstorm) { return; }
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	if (instanceCount_ > 0)
	{
		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		ID3D12PipelineState* pipeline = mode_ == Mode::Rain ? rainPipelineState_.Get() :
			(mode_ == Mode::Snow ? snowPipelineState_.Get() : sandPipelineState_.Get());
		commandList->SetPipelineState(pipeline);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
		commandList->SetGraphicsRootDescriptorTable(0, instanceSrvHandle_);
		commandList->DrawInstanced(6, instanceCount_, 0, 0);
	}
	if (mode_ == Mode::Sandstorm)
	{
		commandList->SetGraphicsRootSignature(rootSignature_.Get());
		commandList->SetPipelineState(sandHazePipelineState_.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->SetGraphicsRootConstantBufferView(1, sandHazeResource_->GetGPUVirtualAddress());
		commandList->DrawInstanced(3, 1, 0, 0);
	}
}

void WeatherSystem::DrawImGuiControls()
{
#ifdef USE_IMGUI
	static constexpr const char* kModeNames[] = { "Off", "Rain", "Snow", "Sandstorm" };
	int modeIndex = static_cast<int>(mode_);
	if (ImGui::Combo("Type", &modeIndex, kModeNames, IM_ARRAYSIZE(kModeNames)))
	{
		mode_ = static_cast<Mode>(modeIndex);
		particles_.fill(WeatherParticle{});
		spawnCredit_ = 0.0f;
		instanceCount_ = 0;
		spawnCursor_ = 0;
	}
	if (mode_ == Mode::Rain)
	{
		ImGui::SliderFloat("Rain / sec", &rainPerSecond_, 0.0f, 1200.0f, "%.0f");
		ImGui::SliderFloat("Rain fall speed", &rainFallSpeed_, 2.0f, 20.0f, "%.1f");
	}
	else if (mode_ == Mode::Snow)
	{
		ImGui::SliderFloat("Snow / sec", &snowPerSecond_, 0.0f, 500.0f, "%.0f");
		ImGui::SliderFloat("Snow fall speed", &snowFallSpeed_, 0.2f, 4.0f, "%.1f");
	}
	else if (mode_ == Mode::Sandstorm)
	{
		ImGui::SliderFloat("Sand / sec", &sandPerSecond_, 0.0f, 3500.0f, "%.0f");
		ImGui::SliderFloat("Sand speed", &sandSpeed_, 0.0f, 12.0f, "%.1f");
		ImGui::SliderFloat("Sand veil opacity", &sandVeilOpacity_, 0.0f, 1.0f, "%.2f");
		ImGui::SliderAngle("Sand direction", &sandDirection_);
	}
	ImGui::SliderFloat("Wind X", &windX_, -6.0f, 6.0f, "%.1f");
	ImGui::SliderFloat("Wind Z", &windZ_, -8.0f, 8.0f, "%.1f");
	ImGui::Text("Active weather particles: %u / %u", instanceCount_, kMaxParticles);
#endif
}

WeatherConditions WeatherSystem::GetConditions() const
{
	// 砂嵐の流れも紙吹雪へ渡す。雨以外のモードでは雨の重みだけを消す。
	const float sandWindX = mode_ == Mode::Sandstorm ? std::cos(sandDirection_) * sandSpeed_ : 0.0f;
	const float sandWindZ = mode_ == Mode::Sandstorm ? std::sin(sandDirection_) * sandSpeed_ : 0.0f;
	return { { windX_ + sandWindX, 0.0f, windZ_ + sandWindZ },
		mode_ == Mode::Rain ? rainPerSecond_ / 1200.0f : 0.0f };
}

void WeatherSystem::Finalize()
{
	if (instanceResource_ && instanceData_)
	{
		instanceResource_->Unmap(0, nullptr);
	}
	instanceData_ = nullptr;
	instanceResource_.Reset();
	if (sandHazeResource_ && sandHazeData_)
	{
		sandHazeResource_->Unmap(0, nullptr);
	}
	sandHazeData_ = nullptr;
	sandHazeResource_.Reset();
	vertexResource_.Reset();
	rainPipelineState_.Reset();
	snowPipelineState_.Reset();
	sandPipelineState_.Reset();
	sandHazePipelineState_.Reset();
	rootSignature_.Reset();
	vertexShaderBlob_.Reset();
	pixelShaderBlob_.Reset();
	snowPixelShaderBlob_.Reset();
	sandPixelShaderBlob_.Reset();
	sandHazeVertexShaderBlob_.Reset();
	sandHazePixelShaderBlob_.Reset();
	dxCommon_ = nullptr;
}
