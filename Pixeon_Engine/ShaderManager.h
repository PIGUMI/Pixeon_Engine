/*
* ファイル名　ShaderManager
* 説　　　明　シェーダーの読み込み・コンパイル・定数バッファの管理
*
*   エディタ: SceneRoot/Shader/hlsl の変更を検出してコンパイルする
*             DX11 / DX12 → DXBC (.cso, d3dcompiler)
*             Vulkan      → SPIR-V (.spv, glslangValidator)
*   ゲーム  : アーカイブ内のコンパイル済みバイナリだけを読む
*   どちらもバイナリは仮想パス shader/cso/ または shader/spv/ から読み込む
*/
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <Windows.h>
#include "RHI/RHI.h"

enum class ShaderStage { VS, PS };

class ShaderManager {
public:
	static ShaderManager* GetInstance();
	static void DestroyInstance();

	void Initialize(RHI::Device* device);
	void Finalize();

	bool CreateHLSLTemplate(const std::string& shaderName, const std::string& type);

	// 変更された HLSL をコンパイルし直して読み込む（エディタのみ）
	void UpdateAndCompileShaders();

	RHI::Shader* GetVertexShader(const std::string& name);
	RHI::Shader* GetPixelShader(const std::string& name);

	bool CreateConstantBuffer(const std::string& key, UINT bufferSize);
	bool WriteBuffer(const std::string& key, UINT slot, void* pData, UINT dataSize);
	RHI::Buffer* GetConstantBuffer(const std::string& key, UINT slot);

	std::vector<std::string> GetShaderList(const std::string& type) const;

	struct VariableDesc {
		std::string name;
		UINT        offset = 0;
		UINT        size = 0;
	};

	struct CBufferRuntime {
		std::string                               name;
		UINT                                      bindPoint = 0;
		UINT                                      size = 0;
		RHI::BufferRef                            gpuBuffer;
		std::vector<uint8_t>                      cpuData;
		bool                                      dirty = false;
		std::unordered_map<std::string, VariableDesc> varsByName;
	};

	struct TextureBindDesc {
		std::string name;
		UINT        bindPoint = 0;
	};

	struct ShaderReflectionData {
		std::vector<CBufferRuntime>  cbuffers;
		std::unordered_map<std::string, size_t> cbufIndexByName;

		std::vector<TextureBindDesc> textures;
		std::unordered_map<std::string, UINT> texBindPointByName;
	};

	const ShaderReflectionData* GetReflection(ShaderStage stage, const std::string& shaderName) const;

	bool SetCBufferVariable(ShaderStage stage,
		const std::string& shaderName,
		const std::string& cbName,
		const std::string& varName,
		const void* data, UINT size);

	bool SetCBufferRaw(ShaderStage stage,
		const std::string& shaderName,
		const std::string& cbName,
		const void* data, UINT size);

	bool CommitAndBind(ShaderStage stage, const std::string& shaderName);

	bool BindSRV(ShaderStage stage,
		const std::string& shaderName,
		const std::string& texVarName,
		RHI::Texture* texture);

private:
	ShaderManager() = default;
	~ShaderManager() {}

	bool IsSpirvBackend() const;
	std::string BinaryPrefixGet() const;
	std::string BinaryExtensionGet() const;
	bool SourceCompile(const std::string& hlslPath, const std::string& shaderName);
	void BinariesLoad();
	bool BinaryLoad(const std::string& virtualPath, const std::string& name);
	void ReflectionBuild(ShaderStage stage, const std::string& shaderName, const RHI::ShaderReflection& reflection);

	RHI::Device* m_cls_Device = nullptr;

	std::unordered_map<std::string, RHI::ShaderRef> m_vsShaders;
	std::unordered_map<std::string, RHI::ShaderRef> m_psShaders;
	std::vector<RHI::ShaderRef> m_retiredShaders;   // ホットリロードで置き換えた古いシェーダー
	std::unordered_map<std::string, std::vector<RHI::BufferRef>> m_constantBuffers;
	std::unordered_map<std::string, ShaderReflectionData> m_vsRef;
	std::unordered_map<std::string, ShaderReflectionData> m_psRef;
	std::unordered_map<std::string, std::filesystem::file_time_type> m_hlslUpdateTimes;
	std::string m_SpirvCompiler;

	static ShaderManager* instance;
};
