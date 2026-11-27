#include "Particle.h"

#include <cassert>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>

#include "DirectXCommon.h"
#include "DirectXResource.h"
#include "Logger.h"
#include "ShaderCompiler.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void Particle::Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler, Logger* logger,
	const std::string& modelName, const std::string& objectName, const std::string& modelDirectory,
	const Vector3& modelScale, ParticleEffectType effectType)
{
	dxCommon_ = dxCommon;
	shaderCompiler_ = shaderCompiler;
	logger_ = logger;
	modelName_ = modelName;
	objectName_ = objectName;
	modelDirectory_ = modelDirectory;
	modelScale_ = modelScale;
	effectType_ = effectType;

	CreateRootSignature();
	CreatePipelineState();
	CreateVertexBuffer();	 // メッシュとマテリアル情報も生成
	CreateConstantBuffers(); // MaterialのConstantBufferを作成
	CreateTexture();
	CreateInstancingResource();
}

void Particle::Finalize()
{
	meshResources_.clear();
	materialResources_.clear();
	instancingResource_.Reset();
	graphicsPipelineState_.Reset();
	pixelShaderBlob_.Reset();
	vertexShaderBlob_.Reset();
	rootSignature_.Reset();
	errorBlob_.Reset();
	signatureBlob_.Reset();
	logger_ = nullptr;
	shaderCompiler_ = nullptr;
	dxCommon_ = nullptr;
	instancingData_ = nullptr;
}

void Particle::EmitBurst(const Vector3& center, uint32_t count, const Vector3& emissionDirection)
{
	assert(effectType_ == ParticleEffectType::Debris || effectType_ == ParticleEffectType::Confetti);
	uint32_t emitted = 0;
	for (ParticleData& particle : particles_)
	{
		if (emitted == count)
		{
			break;
		}
		if (particle.currentTime < particle.lifeTime)
		{
			continue; // 生存中の粒は上書きしない
		}
		particle = MakeNewParticle(randomEngine_, center, emissionDirection);
		++emitted;
	}
}

void Particle::EmitLeafPile(const Vector3& center, uint64_t groupId, uint32_t count)
{
	assert(effectType_ == ParticleEffectType::Leaf);
	uint32_t emitted = 0;
	for (ParticleData& particle : particles_)
	{
		if (emitted == count) { break; }
		if (particle.currentTime < particle.lifeTime) { continue; }
		particle = MakeNewLeaf(randomEngine_, center, groupId);
		++emitted;
	}
	assert(emitted == count);
}

void Particle::EmitFire(const Vector3& center, uint64_t groupId, uint32_t count)
{
	assert(effectType_ == ParticleEffectType::Fire);
	uint32_t emitted = 0;
	for (ParticleData& particle : particles_)
	{
		if (emitted == count) { break; }
		if (particle.currentTime < particle.lifeTime) { continue; }
		particle = MakeNewFire(randomEngine_, center, groupId);
		++emitted;
	}
}

void Particle::ExtinguishFire(uint64_t groupId)
{
	assert(effectType_ == ParticleEffectType::Fire);
	for (ParticleData& particle : particles_)
	{
		if (particle.groupId == groupId) { particle.currentTime = particle.lifeTime; }
	}
}

void Particle::EmitSmoke(const Vector3& center, uint32_t count)
{
	assert(effectType_ == ParticleEffectType::Smoke);
	uint32_t emitted = 0;
	for (ParticleData& particle : particles_)
	{
		if (emitted == count) { break; }
		if (particle.currentTime < particle.lifeTime) { continue; }
		particle = MakeNewSmoke(randomEngine_, center);
		++emitted;
	}
}

void Particle::EmitElectricBarrier(const Vector3& center, uint64_t groupId)
{
	assert(effectType_ == ParticleEffectType::ElectricBarrier);
	for (ParticleData& particle : particles_)
	{
		if (particle.currentTime < particle.lifeTime) { continue; }
		particle = MakeNewElectricBarrier(center, groupId);
		return;
	}
	assert(false); // 障害物の数よりバッファが小さい
}

void Particle::RemoveElectricBarrier(uint64_t groupId)
{
	assert(effectType_ == ParticleEffectType::ElectricBarrier);
	for (ParticleData& particle : particles_)
	{
		if (particle.groupId == groupId) { particle.currentTime = particle.lifeTime; }
	}
}

void Particle::EmitElectricBurst(const Vector3& center, uint32_t count)
{
	assert(effectType_ == ParticleEffectType::ElectricSpark);
	uint32_t emitted = 0;
	for (ParticleData& particle : particles_)
	{
		if (emitted == count) { break; }
		if (particle.currentTime < particle.lifeTime) { continue; }
		particle = MakeNewElectricSpark(randomEngine_, center);
		++emitted;
	}
}

void Particle::EmitElectricFlash(const Vector3& center)
{
	assert(effectType_ == ParticleEffectType::ElectricFlash);
	for (ParticleData& particle : particles_)
	{
		if (particle.currentTime < particle.lifeTime) { continue; }
		particle = MakeNewElectricFlash(center);
		return;
	}
}

void Particle::EmitMagnetVortex(const Vector3& center, uint64_t groupId, uint32_t count)
{
	assert(effectType_ == ParticleEffectType::MagnetShards);
	uint32_t emitted = 0;
	for (ParticleData& particle : particles_)
	{
		if (emitted == count) { break; }
		if (particle.currentTime < particle.lifeTime) { continue; }
		particle = MakeNewMagnetShard(randomEngine_, center, groupId);
		++emitted;
	}
	// 同時に複数の磁力障害物がある場合は空きスロット分だけ生成する。
}

void Particle::ReleaseMagnetGroup(uint64_t groupId)
{
	assert(effectType_ == ParticleEffectType::MagnetShards);
	std::uniform_real_distribution<float> speed(2.5f, 4.0f);
	std::uniform_real_distribution<float> lift(1.0f, 2.4f);
	std::uniform_real_distribution<float> duration(1.1f, 1.7f);
	for (ParticleData& particle : particles_)
	{
		if (particle.groupId != groupId || particle.released ||
			particle.currentTime >= particle.lifeTime) { continue; }
		const float dx = particle.transform.translate.x - particle.orbitCenter.x;
		const float dz = particle.transform.translate.z - particle.orbitCenter.z;
		const float distance = (std::max)(std::sqrt(dx * dx + dz * dz), 0.0001f);
		const float speedScale = effectSettings_ ? effectSettings_->magnet.releaseSpeedScale : 1.0f;
		const float outwardSpeed = speed(randomEngine_) * speedScale;
		particle.velocity = { dx / distance * outwardSpeed, lift(randomEngine_) * speedScale,
			dz / distance * outwardSpeed };
		particle.released = true;
		particle.currentTime = 0.0f;
		particle.lifeTime = duration(randomEngine_);
	}
}

void Particle::RemoveMagnetGroup(uint64_t groupId)
{
	assert(effectType_ == ParticleEffectType::MagnetShards);
	for (ParticleData& particle : particles_)
	{
		if (particle.groupId == groupId) { particle.currentTime = particle.lifeTime; }
	}
}

void Particle::EmitMagnetCore(const Vector3& center, uint64_t groupId)
{
	assert(effectType_ == ParticleEffectType::MagnetCore);
	for (ParticleData& particle : particles_)
	{
		if (particle.currentTime < particle.lifeTime) { continue; }
		particle = MakeNewMagnetCore(center, groupId);
		return;
	}
	assert(false);
}

void Particle::RemoveMagnetCore(uint64_t groupId)
{
	assert(effectType_ == ParticleEffectType::MagnetCore);
	for (ParticleData& particle : particles_)
	{
		if (particle.groupId == groupId) { particle.currentTime = particle.lifeTime; }
	}
}

uint32_t Particle::CountLeavesInPath(uint64_t groupId, const Vector3& center) const
{
	uint32_t count = 0;
	for (const ParticleData& particle : particles_)
	{
		if (particle.groupId == groupId && particle.currentTime < particle.lifeTime &&
			std::abs(particle.transform.translate.x - center.x) < 1.25f &&
			std::abs(particle.transform.translate.z - center.z) < 0.9f)
		{
			++count;
		}
	}
	return count;
}

void Particle::RemoveLeafGroup(uint64_t groupId)
{
	for (ParticleData& particle : particles_)
	{
		if (particle.groupId == groupId) { particle.currentTime = particle.lifeTime; }
	}
}

void Particle::Update(float deltaTime, const WeatherConditions& weather,
	const Vector3& cameraPosition, float drawDistance, uint64_t activeLeafGroup)
{
	assert(instanceCount_ <= kMaxInstanceCount);
	assert(deltaTime >= 0.0f);

	// 毎フレーム描画数を数え直す
	instanceCount_ = 0;

	for (uint32_t i = 0; i < kMaxInstanceCount; ++i)
	{

		ParticleData& particle = particles_[i];

		if (particle.currentTime >= particle.lifeTime)
		{
			continue; // 寿命が切れた粒は描画しない
		}

		if (deltaTime > 0.0f)
		{
			particle.currentTime += deltaTime;

			if (particle.currentTime >= particle.lifeTime)
			{
				continue;
			}

			if (effectType_ == ParticleEffectType::Leaf)
			{
				// 先に並んでいる落ち葉まで吹き飛ばさないよう、現在の障害物だけ風を受ける。
				const Vector3 wind = particle.groupId == activeLeafGroup
					? weather.windVelocity : Vector3{};
				const float response = (std::min)(3.0f * deltaTime, 1.0f);
				particle.velocity.x += (wind.x - particle.velocity.x) * response;
				particle.velocity.z += (wind.z - particle.velocity.z) * response;
				particle.transform.translate.x += particle.velocity.x * deltaTime;
				particle.transform.translate.z += particle.velocity.z * deltaTime;
				const float windSpeed = std::sqrt(particle.velocity.x * particle.velocity.x +
					particle.velocity.z * particle.velocity.z);
				particle.transform.translate.y = particle.baseHeight +
					std::sin(particle.currentTime * 6.0f + particle.flutterPhase) *
					0.12f * (std::min)(windSpeed, 2.0f);
			}
			else if (effectType_ == ParticleEffectType::Fire)
			{
				// 上昇する炎の先端ほど風下へ流れる。
				particle.transform.translate.x +=
					(particle.velocity.x + weather.windVelocity.x * 0.18f +
						0.12f * std::sin(particle.currentTime * 11.0f + particle.flutterPhase)) * deltaTime;
				particle.transform.translate.y += particle.velocity.y *
					(effectSettings_ ? effectSettings_->fire.riseSpeedScale : 1.0f) * deltaTime;
				particle.transform.translate.z +=
					(particle.velocity.z + weather.windVelocity.z * 0.18f) * deltaTime;
				const float ageRatio = particle.currentTime / particle.lifeTime;
				particle.color.w = (std::min)(particle.currentTime * 12.0f, 1.0f) *
					(1.0f - ageRatio) * (1.0f - ageRatio);
			}
			else if (effectType_ == ParticleEffectType::Smoke)
			{
				particle.transform.translate.x +=
					(particle.velocity.x + weather.windVelocity.x * 0.35f +
						0.10f * std::sin(particle.currentTime * 3.0f + particle.flutterPhase)) * deltaTime;
				particle.transform.translate.y += particle.velocity.y * deltaTime;
				particle.transform.translate.z +=
					(particle.velocity.z + weather.windVelocity.z * 0.35f) * deltaTime;
				const float remaining = 1.0f - particle.currentTime / particle.lifeTime;
				particle.color.w = (std::min)(particle.currentTime * 3.5f, 1.0f) *
					remaining * remaining * 0.42f;
			}
			else if (effectType_ == ParticleEffectType::ElectricBarrier)
			{
				// シェーダーへ時刻を渡し、放電の筋を揺らす。
				particle.color.w = particle.currentTime;
			}
			else if (effectType_ == ParticleEffectType::ElectricSpark)
			{
				particle.velocity.y -= 3.0f * deltaTime;
				particle.transform.translate.x += particle.velocity.x * deltaTime;
				particle.transform.translate.y += particle.velocity.y * deltaTime;
				particle.transform.translate.z += particle.velocity.z * deltaTime;
				const float remaining = 1.0f - particle.currentTime / particle.lifeTime;
				particle.color.w = remaining * (std::min)(particle.currentTime * 20.0f, 1.0f);
			}
			else if (effectType_ == ParticleEffectType::ElectricFlash)
			{
				particle.color.w = 1.0f - particle.currentTime / particle.lifeTime;
			}
			else if (effectType_ == ParticleEffectType::MagnetShards)
			{
				if (!particle.released)
				{
					const float orbitSpeedScale = effectSettings_ ? effectSettings_->magnet.orbitSpeedScale : 1.0f;
					const float orbitRadiusScale = effectSettings_ ? effectSettings_->magnet.orbitRadiusScale : 1.0f;
					particle.flutterPhase += particle.orbitSpeed * orbitSpeedScale * deltaTime;
					particle.transform.translate.x = particle.orbitCenter.x +
						particle.orbitRadius * orbitRadiusScale * std::cos(particle.flutterPhase);
					particle.transform.translate.y = particle.orbitCenter.y +
						particle.baseHeight + 0.12f * std::sin(particle.flutterPhase * 2.0f);
					particle.transform.translate.z = particle.orbitCenter.z +
						particle.orbitRadius * orbitRadiusScale * 0.55f * std::sin(particle.flutterPhase);
				}
				else
				{
					particle.velocity.y -= 3.0f * deltaTime;
					particle.transform.translate.x += particle.velocity.x * deltaTime;
					particle.transform.translate.y += particle.velocity.y * deltaTime;
					particle.transform.translate.z += particle.velocity.z * deltaTime;
				}
			}
			else if (effectType_ == ParticleEffectType::MagnetCore)
			{
				particle.color.w = particle.currentTime;
			}
			else
			{
				// 紙吹雪だけが天候の影響を受ける。雨が強いと濡れて落下が速くなる。
				const bool isConfetti = effectType_ == ParticleEffectType::Confetti;
				const float gravity = isConfetti ? 1.2f + 2.0f * weather.rainIntensity : 3.0f;
				particle.velocity.y -= gravity * deltaTime;
				particle.transform.translate.x +=
					(particle.velocity.x + (isConfetti ? weather.windVelocity.x : 0.0f)) * deltaTime;
				particle.transform.translate.y += particle.velocity.y * deltaTime;
				particle.transform.translate.z +=
					(particle.velocity.z + (isConfetti ? weather.windVelocity.z : 0.0f)) * deltaTime;
			}
			particle.transform.rotate.x += particle.angularVelocity.x * deltaTime;
			particle.transform.rotate.y += particle.angularVelocity.y * deltaTime;
			particle.transform.rotate.z += particle.angularVelocity.z * deltaTime;
		}
		if ((effectType_ == ParticleEffectType::Leaf || effectType_ == ParticleEffectType::Fire ||
			effectType_ == ParticleEffectType::Smoke || effectType_ == ParticleEffectType::ElectricBarrier ||
			effectType_ == ParticleEffectType::ElectricSpark || effectType_ == ParticleEffectType::ElectricFlash ||
			effectType_ == ParticleEffectType::MagnetShards || effectType_ == ParticleEffectType::MagnetCore)
			&& drawDistance > 0.0f)
		{
			const float dx = particle.transform.translate.x - cameraPosition.x;
			const float dy = particle.transform.translate.y - cameraPosition.y;
			const float dz = particle.transform.translate.z - cameraPosition.z;
			if (dx * dx + dy * dy + dz * dz > drawDistance * drawDistance) { continue; }
		}
		if (effectType_ == ParticleEffectType::Fire || effectType_ == ParticleEffectType::Smoke ||
			effectType_ == ParticleEffectType::ElectricBarrier || effectType_ == ParticleEffectType::ElectricSpark ||
			effectType_ == ParticleEffectType::ElectricFlash || effectType_ == ParticleEffectType::MagnetCore)
		{
			// 半透明エフェクトの板をカメラに向ける。
			particle.transform.rotate.y = std::atan2(
				cameraPosition.x - particle.transform.translate.x,
				cameraPosition.z - particle.transform.translate.z);
		}

		// 消える直前は小さくして、透明な立体面同士の描画順問題を避ける。
		const float remaining = 1.0f - particle.currentTime / particle.lifeTime;
		const float shrink = (effectType_ == ParticleEffectType::Leaf ||
			effectType_ == ParticleEffectType::Fire || effectType_ == ParticleEffectType::Smoke ||
			effectType_ == ParticleEffectType::ElectricBarrier || effectType_ == ParticleEffectType::ElectricSpark ||
			effectType_ == ParticleEffectType::ElectricFlash || effectType_ == ParticleEffectType::MagnetCore)
			? 1.0f :
			(remaining < 0.25f ? remaining * 4.0f : 1.0f);
		const float growth = effectType_ == ParticleEffectType::Smoke
			? 1.0f + 1.6f * particle.currentTime / particle.lifeTime : 1.0f;
		float effectSizeScale = 1.0f;
		Vector3 effectTint{ 1.0f, 1.0f, 1.0f };
		if (effectSettings_) {
			if (effectType_ == ParticleEffectType::Fire) {
				effectSizeScale = effectSettings_->fire.sizeScale;
				effectTint = effectSettings_->fire.tint;
			} else if (effectType_ == ParticleEffectType::ElectricBarrier ||
				effectType_ == ParticleEffectType::ElectricSpark || effectType_ == ParticleEffectType::ElectricFlash) {
				effectSizeScale = effectSettings_->electric.sizeScale;
				effectTint = effectSettings_->electric.tint;
			} else if (effectType_ == ParticleEffectType::MagnetShards || effectType_ == ParticleEffectType::MagnetCore) {
				effectSizeScale = effectSettings_->magnet.sizeScale;
				effectTint = effectSettings_->magnet.tint;
			}
		}
		const Vector3 renderScale{
			particle.transform.scale.x * shrink * growth * effectSizeScale,
			particle.transform.scale.y * shrink * growth * effectSizeScale,
			particle.transform.scale.z * shrink * effectSizeScale
		};
		const Matrix4x4 world = MatrixUtility::MakeAffineMatrix(
			renderScale,
			particle.transform.rotate,
			particle.transform.translate);

		instancingData_[instanceCount_].World = world;
		instancingData_[instanceCount_].WVP = MatrixUtility::Multiply(world, viewProjection_);
		instancingData_[instanceCount_].WorldInverseTranspose = MatrixUtility::Transpose(MatrixUtility::Inverse(world));
		instancingData_[instanceCount_].color = {
			particle.color.x * effectTint.x, particle.color.y * effectTint.y,
			particle.color.z * effectTint.z, particle.color.w };


		++instanceCount_;
	}

	// Object3dから流用したMaterialのUV更新
	for (MaterialGpuResource& material : materialResources_)
	{
		material.materialData->uvTransform = MatrixUtility::MakeAffineMatrix(
			material.uvTransform.scale,
			material.uvTransform.rotate,
			material.uvTransform.translate);
	}
}

void Particle::Draw()
{
	assert(instanceCount_ <= kMaxInstanceCount);

	if (instanceCount_ == 0)
	{
		return;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// [1] 全メッシュで共有するインスタンスデータのSRV
	commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_);

	for (uint32_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex)
	{
		const MeshData& meshData = modelData_.meshes[meshIndex];
		if (meshData.vertices.empty())
		{
			continue;
		}
		assert(meshData.materialIndex < materialResources_.size());
		const MeshGpuResource& mesh = meshResources_[meshIndex];
		const MaterialGpuResource& material = materialResources_[meshData.materialIndex];
		commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
		commandList->SetGraphicsRootConstantBufferView(0, material.materialResource->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(2, material.textureSrvHandleGPU);
		commandList->DrawInstanced(static_cast<UINT>(meshData.vertices.size()), instanceCount_, 0, 0);
	}
}

void Particle::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Settings");
	ImGui::PushID(objectName_.c_str());
	if (ImGui::CollapsingHeader(objectName_.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Translate", &transform_.translate.x, 0.01f);
		ImGui::DragFloat3("Rotate (rad)", &transform_.rotate.x, 0.01f);
		ImGui::DragFloat3("Scale", &transform_.scale.x, 0.01f, 0.01f, 100.0f);

		if (ImGui::TreeNode("Materials"))
		{
			static constexpr const char* kLightingModeNames[] = { "Lighting None", "Lambert", "Half Lambert" };
			for (uint32_t materialIndex = 0; materialIndex < materialResources_.size(); ++materialIndex) {
				MaterialGpuResource& material = materialResources_[materialIndex];
				const std::string& materialName = modelData_.materials[materialIndex].name;
				ImGui::PushID(static_cast<int>(materialIndex));
				if (ImGui::TreeNode(materialName.empty() ? "Material" : materialName.c_str()))
				{
					int lightingMode = material.materialData->lightingMode;
					if (ImGui::Combo("Lighting", &lightingMode, kLightingModeNames, IM_ARRAYSIZE(kLightingModeNames)))
					{
						material.materialData->lightingMode = lightingMode;
					}

					ImGui::ColorEdit4("Color", &material.materialData->color.x);
					ImGui::DragFloat2("UV Translate", &material.uvTransform.translate.x, 0.01f);
					ImGui::DragFloat2("UV Scale", &material.uvTransform.scale.x, 0.01f, 0.01f, 100.0f);
					ImGui::SliderAngle("UV Rotate", &material.uvTransform.rotate.z);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
	}

	ImGui::PopID();
	ImGui::End();
#endif
}

void Particle::CreateRootSignature()
{
	// PSが読むテクスチャ用SRV
	D3D12_DESCRIPTOR_RANGE textureRange{};
	textureRange.BaseShaderRegister = 0; // PSのt0
	textureRange.NumDescriptors = 1;
	textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// VSが読むインスタンスデータ用SRV
	D3D12_DESCRIPTOR_RANGE instancingRange{};
	instancingRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	instancingRange.NumDescriptors = 1;
	instancingRange.BaseShaderRegister = 0; // VSのt0
	instancingRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3]{};

	// [0] Material
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// [1] インスタンスデータ
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &instancingRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

	// [2] テクスチャ
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &textureRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC staticSampler{};
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
	staticSampler.ShaderRegister = 0;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pStaticSamplers = &staticSampler;
	rootSignatureDesc.NumStaticSamplers = 1;

	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr))
	{
		logger_->Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void Particle::CreatePipelineState()
{
	D3D12_INPUT_ELEMENT_DESC inputElements[3] = {};
	inputElements[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElements[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElements[2] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	D3D12_INPUT_LAYOUT_DESC inputLayout{ inputElements, _countof(inputElements) };

	// 破片は不透明にして深度を書き、立体の各面の前後関係を正しく描く。
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	if (effectType_ == ParticleEffectType::Fire ||
		effectType_ == ParticleEffectType::ElectricBarrier ||
		effectType_ == ParticleEffectType::ElectricSpark || effectType_ == ParticleEffectType::ElectricFlash ||
		effectType_ == ParticleEffectType::MagnetCore)
	{
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	}
	else if (effectType_ == ParticleEffectType::Smoke)
	{
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	}

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = effectType_ == ParticleEffectType::Leaf ||
		effectType_ == ParticleEffectType::Fire || effectType_ == ParticleEffectType::Smoke ||
		effectType_ == ParticleEffectType::ElectricBarrier || effectType_ == ParticleEffectType::ElectricSpark ||
		effectType_ == ParticleEffectType::ElectricFlash || effectType_ == ParticleEffectType::MagnetCore
		? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	vertexShaderBlob_ = shaderCompiler_->Compile(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
	const wchar_t* pixelShader = L"resources/shaders/Particle.PS.hlsl";
	switch (effectType_)
	{
	case ParticleEffectType::Leaf: pixelShader = L"resources/shaders/Particle.Leaf.PS.hlsl"; break;
	case ParticleEffectType::Fire: pixelShader = L"resources/shaders/Particle.Fire.PS.hlsl"; break;
	case ParticleEffectType::Smoke: pixelShader = L"resources/shaders/Particle.Smoke.PS.hlsl"; break;
	case ParticleEffectType::ElectricBarrier: pixelShader = L"resources/shaders/Particle.ElectricBarrier.PS.hlsl"; break;
	case ParticleEffectType::ElectricSpark: pixelShader = L"resources/shaders/Particle.ElectricSpark.PS.hlsl"; break;
	case ParticleEffectType::ElectricFlash: pixelShader = L"resources/shaders/Particle.ElectricFlash.PS.hlsl"; break;
	case ParticleEffectType::MagnetCore: pixelShader = L"resources/shaders/Particle.MagnetCore.PS.hlsl"; break;
	default: break;
	}
	pixelShaderBlob_ = shaderCompiler_->Compile(pixelShader, L"ps_6_0");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	// Particle自身のRootSignatureを指定する
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.InputLayout = inputLayout;
	pipelineDesc.VS = { vertexShaderBlob_->GetBufferPointer(), vertexShaderBlob_->GetBufferSize() };
	pipelineDesc.PS = { pixelShaderBlob_->GetBufferPointer(), pixelShaderBlob_->GetBufferSize() };
	pipelineDesc.BlendState = blendDesc;
	pipelineDesc.RasterizerState = rasterizerDesc;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = dxCommon_->GetRenderTargetFormat();
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// 不透明な破片同士の前後関係も深度で判定する。
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = effectType_ == ParticleEffectType::Fire ||
		effectType_ == ParticleEffectType::Smoke ||
		effectType_ == ParticleEffectType::ElectricBarrier || effectType_ == ParticleEffectType::ElectricSpark ||
		effectType_ == ParticleEffectType::ElectricFlash || effectType_ == ParticleEffectType::MagnetCore
		? D3D12_DEPTH_WRITE_MASK_ZERO : D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	pipelineDesc.DepthStencilState = depthStencilDesc;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	const HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

void Particle::CreateConstantBuffers()
{

	materialResources_.resize(modelData_.materials.size());
	for (MaterialGpuResource& material : materialResources_)
	{
		material.materialResource = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(Material));
		material.materialResource->Map(0, nullptr, reinterpret_cast<void**>(&material.materialData));
		material.materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		material.materialData->lightingMode = static_cast<int32_t>(
			effectType_ == ParticleEffectType::Confetti || effectType_ == ParticleEffectType::Fire ||
			effectType_ == ParticleEffectType::Smoke ||
			effectType_ == ParticleEffectType::ElectricBarrier || effectType_ == ParticleEffectType::ElectricSpark ||
			effectType_ == ParticleEffectType::ElectricFlash || effectType_ == ParticleEffectType::MagnetCore
			? LightingMode::None : LightingMode::HalfLambert);
		material.materialData->uvTransform = MatrixUtility::MakeIdentity4x4();
	}

}

void Particle::CreateTexture()
{
	for (uint32_t materialIndex = 0; materialIndex < modelData_.materials.size(); ++materialIndex)
	{
		const MaterialData& materialData = modelData_.materials[materialIndex];
		MaterialGpuResource& materialResource = materialResources_[materialIndex];
		DirectX::ScratchImage mipImages = DirectXResource::LoadTexture(materialData.textureFilePath);
		const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
		materialResource.textureResource = DirectXResource::CreateTextureResource(dxCommon_->GetDevice(), metadata);
		DirectXResource::UploadTextureData(materialResource.textureResource.Get(), mipImages);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = metadata.format;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);
		const uint32_t srvIndex = dxCommon_->AllocateSrvIndex();
		dxCommon_->GetDevice()->CreateShaderResourceView(materialResource.textureResource.Get(), &srvDesc, dxCommon_->GetSrvCpuHandle(srvIndex));
		materialResource.textureSrvHandleGPU = dxCommon_->GetSrvGpuHandle(srvIndex);
	}
}

void Particle::CreateVertexBuffer()
{
	if (modelName_ == "quad")
	{
		CreateQuadVertices();
	}
	else if (modelName_ == "sphere")
	{
		CreateSphereVertices();
	}
	else
	{
		modelData_ = ModelLoader::LoadObjFile(modelDirectory_, modelName_);
	}
	assert(!modelData_.meshes.empty() && !modelData_.materials.empty());

	meshResources_.resize(modelData_.meshes.size());
	for (uint32_t meshIndex = 0; meshIndex < modelData_.meshes.size(); ++meshIndex)
	{
		const MeshData& mesh = modelData_.meshes[meshIndex];
		MeshGpuResource& meshResource = meshResources_[meshIndex];
		if (mesh.vertices.empty())
		{
			continue;
		}
		meshResource.vertexResource = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * mesh.vertices.size());
		meshResource.vertexBufferView.BufferLocation = meshResource.vertexResource->GetGPUVirtualAddress();
		meshResource.vertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * mesh.vertices.size());
		meshResource.vertexBufferView.StrideInBytes = sizeof(VertexData);

		VertexData* vertexData = nullptr;
		meshResource.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
		std::memcpy(vertexData, mesh.vertices.data(), sizeof(VertexData) * mesh.vertices.size());
	}
}

void Particle::CreateSphereVertices()
{
	constexpr uint32_t kSubdivisionX = 32;
	constexpr uint32_t kSubdivisionY = 16;
	constexpr float kPi = 3.14159265f;

	modelData_ = {};
	modelData_.materials.push_back({ "Sphere Material", "resources/uvChecker.png" });
	modelData_.meshes.push_back({ "Sphere", {}, 0 });
	MeshData& sphere = modelData_.meshes.back();
	sphere.vertices.reserve(kSubdivisionX * kSubdivisionY * 6);

	for (uint32_t y = 0; y < kSubdivisionY; ++y)
	{
		const float v0 = static_cast<float>(y) / static_cast<float>(kSubdivisionY);
		const float v1 = static_cast<float>(y + 1) / static_cast<float>(kSubdivisionY);
		const float phi0 = v0 * kPi;
		const float phi1 = v1 * kPi;
		for (uint32_t x = 0; x < kSubdivisionX; ++x)
		{
			const float u0 = static_cast<float>(x) / static_cast<float>(kSubdivisionX);
			const float u1 = static_cast<float>(x + 1) / static_cast<float>(kSubdivisionX);
			const float theta0 = u0 * 2.0f * kPi;
			const float theta1 = u1 * 2.0f * kPi;
			auto makeVertex = [](float phi, float theta, float u, float v) {
				const Vector3 normal = { std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta) };
				return VertexData{ { normal.x, normal.y, normal.z, 1.0f }, { u, v }, normal };
				};

			const VertexData topLeft = makeVertex(phi0, theta0, u0, v0);
			const VertexData topRight = makeVertex(phi0, theta1, u1, v0);
			const VertexData bottomLeft = makeVertex(phi1, theta0, u0, v1);
			const VertexData bottomRight = makeVertex(phi1, theta1, u1, v1);
			sphere.vertices.insert(sphere.vertices.end(), { topLeft, topRight, bottomLeft, topRight, bottomRight, bottomLeft });
		}
	}
}

void Particle::CreateQuadVertices()
{
	modelData_ = {};

	modelData_.materials.push_back({ "Particle Material", "resources/circle.png" });

	modelData_.meshes.push_back({ "Particle Quad", {}, 0 });

	auto& vertices = modelData_.meshes[0].vertices;

	vertices =
	{
		{ { 1.0f,  1.0f, 0.0f, 1.0f},
		  {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },

		{ {-1.0f,  1.0f, 0.0f, 1.0f},
		  {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },

		{ { 1.0f, -1.0f, 0.0f, 1.0f},
		  {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },

		{ { 1.0f, -1.0f, 0.0f, 1.0f},
		  {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },

		{ {-1.0f,  1.0f, 0.0f, 1.0f},
		  {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },

		{ {-1.0f, -1.0f, 0.0f, 1.0f},
		  {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },
	};
}

ParticleData Particle::MakeNewParticle(std::mt19937& randomEngine, const Vector3& center,
	const Vector3& emissionDirection)
{
	ParticleData particle{};
	const bool isConfetti = effectType_ == ParticleEffectType::Confetti;
	std::uniform_real_distribution<float> sizeDistribution(
		isConfetti ? 0.035f : 0.06f,
		isConfetti ? 0.065f : 0.12f);
	const float size = sizeDistribution(randomEngine);
	particle.transform.scale = {
		size * modelScale_.x,
		size * modelScale_.y,
		size * modelScale_.z
	};
	std::uniform_real_distribution<float> angleDistribution(-3.14159265f, 3.14159265f);
	std::uniform_real_distribution<float> spinDistribution(
		isConfetti ? -10.0f : -6.0f, isConfetti ? 10.0f : 6.0f);
	particle.transform.rotate = {
		angleDistribution(randomEngine), angleDistribution(randomEngine), angleDistribution(randomEngine)
	};
	particle.angularVelocity = {
		spinDistribution(randomEngine), spinDistribution(randomEngine), spinDistribution(randomEngine)
	};

	std::uniform_real_distribution<float> offsetDistribution(
		isConfetti ? -0.06f : -0.25f, isConfetti ? 0.06f : 0.25f);
	particle.transform.translate = {
		center.x + offsetDistribution(randomEngine),
		center.y + offsetDistribution(randomEngine),
		center.z + offsetDistribution(randomEngine)
	};

	Vector3 direction{};
	if (isConfetti)
	{
		// クラッカーの口の向きを中心に、円錐状に飛ばす。
		std::uniform_real_distribution<float> spread(-0.45f, 0.45f);
		direction = {
			emissionDirection.x + spread(randomEngine),
			emissionDirection.y + spread(randomEngine),
			emissionDirection.z
		};
	}
	else
	{
		std::uniform_real_distribution<float> spread(-1.0f, 1.0f);
		direction = { spread(randomEngine), spread(randomEngine) + 0.35f, spread(randomEngine) };
	}
	const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
	if (length < 0.001f)
	{
		direction = { 0.0f, 1.0f, 0.0f };
	}
	else
	{
		direction.x /= length;
		direction.y /= length;
		direction.z /= length;
	}
	std::uniform_real_distribution<float> speedDistribution(
		isConfetti ? 2.5f : 1.5f, isConfetti ? 4.5f : 3.5f);
	const float speed = speedDistribution(randomEngine);
	particle.velocity = { direction.x * speed, direction.y * speed, direction.z * speed };

	if (isConfetti)
	{
		static constexpr std::array<Vector4, 7> kColors = {
			Vector4{ 1.0f, 0.18f, 0.25f, 1.0f },
			Vector4{ 1.0f, 0.62f, 0.12f, 1.0f },
			Vector4{ 1.0f, 0.92f, 0.18f, 1.0f },
			Vector4{ 0.18f, 0.9f, 0.35f, 1.0f },
			Vector4{ 0.15f, 0.55f, 1.0f, 1.0f },
			Vector4{ 0.95f, 0.3f, 0.85f, 1.0f },
			Vector4{ 0.65f, 0.35f, 1.0f, 1.0f },
		};
		std::uniform_int_distribution<size_t> colorIndex(0, kColors.size() - 1);
		particle.color = kColors[colorIndex(randomEngine)];
	}
	else
	{
		std::uniform_real_distribution<float> shadeDistribution(0.25f, 0.35f);
		const float shade = shadeDistribution(randomEngine);
		particle.color = { shade, shade, shade, 1.0f };
	}

	std::uniform_real_distribution<float> lifeDistribution(
		isConfetti ? 2.0f : 0.7f, isConfetti ? 3.0f : 1.4f);
	particle.lifeTime = lifeDistribution(randomEngine);
	particle.currentTime = 0.0f;

	return particle;
}

ParticleData Particle::MakeNewLeaf(std::mt19937& randomEngine, const Vector3& center, uint64_t groupId)
{
	ParticleData particle{};
	std::uniform_real_distribution<float> xOffset(-0.85f, 0.85f);
	std::uniform_real_distribution<float> zOffset(-0.35f, 0.35f);
	std::uniform_real_distribution<float> height(0.12f, 0.48f);
	std::uniform_real_distribution<float> size(0.38f, 0.58f);
	std::uniform_real_distribution<float> angle(-3.14159265f, 3.14159265f);
	std::uniform_real_distribution<float> spin(-2.0f, 2.0f);
	const float scale = size(randomEngine);
	particle.transform.scale = { scale, scale, scale };
	particle.transform.translate = { center.x + xOffset(randomEngine),
		center.y + height(randomEngine), center.z + zOffset(randomEngine) };
	particle.transform.rotate = { -0.9f + 0.25f * spin(randomEngine),
		angle(randomEngine), angle(randomEngine) };
	particle.angularVelocity = { spin(randomEngine), spin(randomEngine), spin(randomEngine) };
	static constexpr std::array<Vector4, 5> kColors = {
		Vector4{ 0.85f, 0.34f, 0.10f, 1.0f },
		Vector4{ 0.95f, 0.52f, 0.10f, 1.0f },
		Vector4{ 0.72f, 0.23f, 0.09f, 1.0f },
		Vector4{ 0.96f, 0.68f, 0.17f, 1.0f },
		Vector4{ 0.55f, 0.32f, 0.12f, 1.0f },
	};
	std::uniform_int_distribution<size_t> colorIndex(0, kColors.size() - 1);
	particle.color = kColors[colorIndex(randomEngine)];
	particle.lifeTime = 1000000.0f; // 障害物を破棄するまで維持する
	particle.currentTime = 0.0f;
	particle.groupId = groupId;
	particle.baseHeight = particle.transform.translate.y;
	particle.flutterPhase = angle(randomEngine);
	return particle;
}

ParticleData Particle::MakeNewFire(std::mt19937& randomEngine, const Vector3& center, uint64_t groupId)
{
	ParticleData particle{};
	std::uniform_real_distribution<float> spread(-0.32f, 0.32f);
	std::uniform_real_distribution<float> depth(-0.16f, 0.16f);
	std::uniform_real_distribution<float> width(0.18f, 0.32f);
	std::uniform_real_distribution<float> height(0.45f, 0.68f);
	std::uniform_real_distribution<float> rise(0.35f, 0.8f);
	std::uniform_real_distribution<float> duration(0.65f, 1.05f);
	std::uniform_real_distribution<float> tilt(-0.17f, 0.17f);
	const float particleWidth = width(randomEngine);
	particle.transform.scale = { particleWidth, height(randomEngine), 1.0f };
	particle.transform.rotate = { 0.0f, 0.0f, tilt(randomEngine) };
	particle.transform.translate = { center.x + spread(randomEngine),
		center.y + 0.48f, center.z + depth(randomEngine) };
	particle.velocity = { spread(randomEngine) * 0.2f, rise(randomEngine),
		depth(randomEngine) * 0.2f };
	particle.angularVelocity = { 0.0f, 0.0f, tilt(randomEngine) };
	static constexpr std::array<Vector4, 4> kColors = {
		Vector4{ 1.0f, 0.85f, 0.35f, 1.0f },
		Vector4{ 1.0f, 0.56f, 0.12f, 1.0f },
		Vector4{ 1.0f, 0.32f, 0.06f, 1.0f },
		Vector4{ 1.0f, 0.68f, 0.20f, 1.0f },
	};
	std::uniform_int_distribution<size_t> colorIndex(0, kColors.size() - 1);
	particle.color = kColors[colorIndex(randomEngine)];
	particle.lifeTime = duration(randomEngine) *
		(effectSettings_ ? effectSettings_->fire.lifetimeScale : 1.0f);
	particle.currentTime = 0.0f;
	particle.groupId = groupId;
	std::uniform_real_distribution<float> phase(0.0f, 6.2831853f);
	particle.flutterPhase = phase(randomEngine);
	return particle;
}

ParticleData Particle::MakeNewSmoke(std::mt19937& randomEngine, const Vector3& center)
{
	ParticleData particle{};
	std::uniform_real_distribution<float> horizontal(-0.3f, 0.3f);
	std::uniform_real_distribution<float> startHeight(0.25f, 0.75f);
	std::uniform_real_distribution<float> size(0.18f, 0.31f);
	std::uniform_real_distribution<float> rise(0.45f, 0.85f);
	std::uniform_real_distribution<float> duration(1.7f, 2.5f);
	std::uniform_real_distribution<float> angle(-0.5f, 0.5f);
	std::uniform_real_distribution<float> phase(0.0f, 6.2831853f);
	std::uniform_real_distribution<float> gray(0.30f, 0.47f);
	const float particleSize = size(randomEngine);
	const float shade = gray(randomEngine);
	particle.transform.scale = { particleSize, particleSize, 1.0f };
	particle.transform.rotate = { 0.0f, 0.0f, angle(randomEngine) };
	particle.transform.translate = { center.x + horizontal(randomEngine),
		center.y + startHeight(randomEngine), center.z + horizontal(randomEngine) * 0.5f };
	particle.velocity = { horizontal(randomEngine) * 0.5f, rise(randomEngine),
		horizontal(randomEngine) * 0.25f };
	particle.angularVelocity = { 0.0f, 0.0f, angle(randomEngine) * 0.3f };
	particle.color = { shade, shade, shade * 1.05f, 0.0f };
	particle.lifeTime = duration(randomEngine);
	particle.currentTime = 0.0f;
	particle.flutterPhase = phase(randomEngine);
	return particle;
}

ParticleData Particle::MakeNewElectricBarrier(const Vector3& center, uint64_t groupId)
{
	ParticleData particle{};
	particle.transform.scale = { 0.85f, 0.72f, 1.0f };
	particle.transform.rotate = { 0.0f, 0.0f, 0.0f };
	particle.transform.translate = { center.x, center.y + 0.78f, center.z };
	particle.color = { 0.35f, 0.85f, 1.0f, 0.0f };
	particle.lifeTime = 1000000.0f;
	particle.currentTime = 0.0f;
	particle.groupId = groupId;
	return particle;
}

ParticleData Particle::MakeNewElectricSpark(std::mt19937& randomEngine, const Vector3& center)
{
	ParticleData particle{};
	std::uniform_real_distribution<float> width(0.035f, 0.075f);
	std::uniform_real_distribution<float> length(0.15f, 0.34f);
	std::uniform_real_distribution<float> xOffset(-0.75f, 0.75f);
	std::uniform_real_distribution<float> yOffset(0.15f, 1.45f);
	std::uniform_real_distribution<float> zOffset(-0.13f, 0.13f);
	std::uniform_real_distribution<float> sideSpeed(-3.5f, 3.5f);
	std::uniform_real_distribution<float> upSpeed(-0.8f, 3.8f);
	std::uniform_real_distribution<float> depthSpeed(-1.6f, 1.6f);
	std::uniform_real_distribution<float> duration(0.45f, 0.85f);
	std::uniform_real_distribution<float> spin(-5.0f, 5.0f);
	particle.transform.scale = { width(randomEngine), length(randomEngine), 1.0f };
	particle.transform.translate = { center.x + xOffset(randomEngine),
		center.y + yOffset(randomEngine), center.z + zOffset(randomEngine) };
	particle.velocity = { sideSpeed(randomEngine), upSpeed(randomEngine), depthSpeed(randomEngine) };
	const float speedScale = effectSettings_ ? effectSettings_->electric.speedScale : 1.0f;
	particle.velocity.x *= speedScale;
	particle.velocity.y *= speedScale;
	particle.velocity.z *= speedScale;
	particle.transform.rotate = { 0.0f, 0.0f,
		std::atan2(-particle.velocity.x, particle.velocity.y) };
	particle.angularVelocity = { 0.0f, 0.0f, spin(randomEngine) };
	static constexpr std::array<Vector4, 4> kSparkColors = {
		Vector4{ 0.75f, 0.98f, 1.0f, 1.0f },
		Vector4{ 0.35f, 0.82f, 1.0f, 1.0f },
		Vector4{ 0.72f, 0.58f, 1.0f, 1.0f },
		Vector4{ 0.95f, 0.98f, 1.0f, 1.0f },
	};
	std::uniform_int_distribution<size_t> colorIndex(0, kSparkColors.size() - 1);
	particle.color = kSparkColors[colorIndex(randomEngine)];
	particle.lifeTime = duration(randomEngine) *
		(effectSettings_ ? effectSettings_->electric.lifetimeScale : 1.0f);
	particle.currentTime = 0.0f;
	return particle;
}

ParticleData Particle::MakeNewElectricFlash(const Vector3& center)
{
	ParticleData particle{};
	particle.transform.scale = { 0.95f, 0.95f, 1.0f };
	particle.transform.rotate = { 0.0f, 0.0f, 0.0f };
	particle.transform.translate = { center.x, center.y + 0.78f, center.z - 0.08f };
	particle.color = { 0.55f, 0.88f, 1.0f, 1.0f };
	particle.lifeTime = 0.42f;
	particle.currentTime = 0.0f;
	return particle;
}

ParticleData Particle::MakeNewMagnetShard(std::mt19937& randomEngine,
	const Vector3& center, uint64_t groupId)
{
	ParticleData particle{};
	std::uniform_real_distribution<float> size(0.065f, 0.12f);
	std::uniform_real_distribution<float> radius(0.38f, 0.85f);
	std::uniform_real_distribution<float> height(0.25f, 1.35f);
	std::uniform_real_distribution<float> phase(-3.14159265f, 3.14159265f);
	std::uniform_real_distribution<float> speed(2.2f, 3.8f);
	std::uniform_real_distribution<float> spin(-5.0f, 5.0f);
	const float shardSize = size(randomEngine);
	particle.transform.scale = { shardSize, shardSize, shardSize };
	particle.transform.rotate = { phase(randomEngine), phase(randomEngine), phase(randomEngine) };
	particle.angularVelocity = { spin(randomEngine), spin(randomEngine), spin(randomEngine) };
	particle.orbitCenter = center;
	particle.orbitRadius = radius(randomEngine);
	particle.baseHeight = height(randomEngine);
	particle.flutterPhase = phase(randomEngine);
	particle.orbitSpeed = speed(randomEngine);
	particle.transform.translate = {
		center.x + particle.orbitRadius * std::cos(particle.flutterPhase),
		center.y + particle.baseHeight,
		center.z + particle.orbitRadius * 0.55f * std::sin(particle.flutterPhase)
	};
	static constexpr std::array<Vector4, 3> kMetalColors = {
		Vector4{ 0.88f, 0.92f, 1.0f, 1.0f },
		Vector4{ 0.65f, 0.75f, 0.9f, 1.0f },
		Vector4{ 0.7f, 0.8f, 1.0f, 1.0f },
	};
	std::uniform_int_distribution<size_t> colorIndex(0, kMetalColors.size() - 1);
	particle.color = kMetalColors[colorIndex(randomEngine)];
	particle.lifeTime = 1000000.0f;
	particle.currentTime = 0.0f;
	particle.groupId = groupId;
	return particle;
}

ParticleData Particle::MakeNewMagnetCore(const Vector3& center, uint64_t groupId)
{
	ParticleData particle{};
	particle.transform.scale = { 0.55f, 0.55f, 1.0f };
	particle.transform.rotate = { 0.0f, 0.0f, 0.0f };
	particle.transform.translate = { center.x, center.y + 0.82f, center.z };
	particle.color = { 1.0f, 1.0f, 1.0f, 0.0f };
	particle.lifeTime = 1000000.0f;
	particle.currentTime = 0.0f;
	particle.groupId = groupId;
	return particle;
}

void Particle::CreateInstancingResource()
{
	instancingResource_ = DirectXResource::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ParticleForGPU) * kMaxInstanceCount);

	const HRESULT hr = instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));
	assert(SUCCEEDED(hr));
	particles_.fill(ParticleData{});
	instanceCount_ = 0;

	for (uint32_t i = 0; i < kMaxInstanceCount; ++i)
	{
		instancingData_[i].WVP = MatrixUtility::MakeIdentity4x4();
		instancingData_[i].World = MatrixUtility::MakeIdentity4x4();
		instancingData_[i].WorldInverseTranspose = MatrixUtility::MakeIdentity4x4();
		instancingData_[i].color = { 1.0f, 1.0f, 1.0f, 1.0f };

	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = kMaxInstanceCount;
	srvDesc.Buffer.StructureByteStride = static_cast<UINT>(sizeof(ParticleForGPU));
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	const uint32_t srvIndex = dxCommon_->AllocateSrvIndex();

	dxCommon_->GetDevice()->CreateShaderResourceView(instancingResource_.Get(), &srvDesc, dxCommon_->GetSrvCpuHandle(srvIndex));

	instancingSrvHandleGPU_ = dxCommon_->GetSrvGpuHandle(srvIndex);
}
