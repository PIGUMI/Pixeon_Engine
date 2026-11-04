/*
* ファイル名　ShaderManager
* 説　　　明　シェーダー管理の実装
*/
#include "ShaderManager.h"
#include "AssetManager.h"
#include "MainFrame.h"
#include "SettingManager.h"
#include "../Common/FileUtil.h"
#include "../Common/ShaderCompiler.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>
#include <fstream>

#pragma comment(lib, "d3dcompiler.lib")

ShaderManager* ShaderManager::instance = nullptr;

namespace {
	UINT Align16(UINT v) { return (v + 15) & ~15u; }

	RHI::ShaderStage StageConvert(ShaderStage s)
	{
		return (s == ShaderStage::VS) ? RHI::ShaderStage::Vertex : RHI::ShaderStage::Pixel;
	}

	/*
	* 関数名　DxbcCompile
	* 引　数　hlslPath：入力　target：vs_5_0 / ps_5_0　csoPath：出力
	* 戻り値　成功なら true
	* 説　明　d3dcompiler で HLSL を DXBC にコンパイルしてファイルへ保存する
	*/
	bool DxbcCompile(const std::string& hlslPath, const char* target, const std::string& csoPath)
	{
		ID3DBlob* code = nullptr;
		ID3DBlob* error = nullptr;
		const std::wstring wpath = FileUtil::PathToNative(hlslPath).wstring();
		HRESULT hr = D3DCompileFromFile(wpath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main", target, 0, 0, &code, &error);
		if (FAILED(hr)) {
			if (error) {
				MessageBoxA(nullptr, (char*)error->GetBufferPointer(), "HLSL Compile Error", MB_ICONERROR);
				error->Release();
			}
			if (code) code->Release();
			return false;
		}
		if (error) error->Release();
		const bool ok = FileUtil::FileWriteAll(csoPath, code->GetBufferPointer(), code->GetBufferSize());
		code->Release();
		return ok;
	}
}

ShaderManager* ShaderManager::GetInstance() {
	if (instance == nullptr) {
		instance = new ShaderManager();
	}
	return instance;
}

void ShaderManager::DestroyInstance() {
	if (instance) {
		instance->Finalize();
		delete instance;
		instance = nullptr;
	}
}

/*
* 関数名　Initialize
* 引　数　device：RHI デバイス
* 戻り値　なし
* 説　明　エディタなら HLSL をコンパイルし、その後バイナリを全て読み込む
*/
void ShaderManager::Initialize(RHI::Device* device) {
	m_cls_Device = device;
	if (IsSpirvBackend()) m_SpirvCompiler = ShaderCompiler::SpirvCompilerFind();
	UpdateAndCompileShaders();
	BinariesLoad();
}

void ShaderManager::Finalize() {
	m_vsShaders.clear();
	m_psShaders.clear();
	m_vsRef.clear();
	m_psRef.clear();
	m_constantBuffers.clear();
	m_hlslUpdateTimes.clear();
	m_retiredShaders.clear();
}

bool ShaderManager::IsSpirvBackend() const
{
	return m_cls_Device && m_cls_Device->BackendGet() == RHI::Backend::Vulkan;
}

std::string ShaderManager::BinaryPrefixGet() const
{
	return std::string(VfsPrefix::SHADER) + (IsSpirvBackend() ? "spv/" : "cso/");
}

std::string ShaderManager::BinaryExtensionGet() const
{
	return IsSpirvBackend() ? ".spv" : ".cso";
}

bool ShaderManager::CreateHLSLTemplate(const std::string& shaderName, const std::string& type) {
	std::string path = SettingManager::GetInstance()->GetShaderFilePath() + shaderName + ".hlsl";
	std::ofstream ofs(path);
	if (!ofs) return false;

	if (type == "VS") {
		ofs << "cbuffer CameraCB : register(b0)\n"
			"{\n"
			"    matrix world;\n"
			"    matrix view;\n"
			"    matrix proj;\n"
			"};\n"
			"\n"
			"struct VS_INPUT {\n"
			"    float3 pos   : POSITION;\n"
			"    float4 color : COLOR0;\n"
			"};\n"
			"struct VS_OUTPUT {\n"
			"    float4 pos   : SV_POSITION;\n"
			"    float4 color : COLOR0;\n"
			"};\n"
			"VS_OUTPUT main(VS_INPUT input) {\n"
			"    VS_OUTPUT o;\n"
			"    float4 p = float4(input.pos, 1.0f);\n"
			"    p = mul(p, world);\n"
			"    p = mul(p, view);\n"
			"    o.pos = mul(p, proj);\n"
			"    o.color = input.color;\n"
			"    return o;\n"
			"}\n";
	}
	else if (type == "PS") {
		ofs << "cbuffer MaterialCB : register(b0)\n"
			"{\n"
			"    float4 LineColor;\n"
			"};\n"
			"struct PS_INPUT {\n"
			"    float4 pos   : SV_POSITION;\n"
			"    float4 color : COLOR0;\n"
			"};\n"
			"float4 main(PS_INPUT input) : SV_Target {\n"
			"    return input.color * LineColor;\n"
			"}\n";
	}
	return true;
}

/*
* 関数名　SourceCompile
* 引　数　hlslPath：HLSL のパス　shaderName：シェーダー名 (VS_xxx / PS_xxx)
* 戻り値　成功なら true
* 説　明　現在のバックエンド用のバイナリを生成する
*/
bool ShaderManager::SourceCompile(const std::string& hlslPath, const std::string& shaderName)
{
	ShaderBindings::Stage stage{};
	if (!ShaderCompiler::StageFromName(shaderName, stage)) return false;
	SettingManager* settings = SettingManager::GetInstance();
	if (!IsSpirvBackend()) {
		const char* target = (stage == ShaderBindings::Stage::Vertex) ? "vs_5_0" : "ps_5_0";
		return DxbcCompile(hlslPath, target, settings->GetCSOFilePath() + shaderName + ".cso");
	}
	if (m_SpirvCompiler.empty()) return false;   // コンパイラが無ければ同梱の .spv を使う
	std::string log;
	if (!ShaderCompiler::SpirvCompile(m_SpirvCompiler, hlslPath, stage, settings->GetSPIRVFilePath() + shaderName + ".spv", log)) {
		MessageBoxA(nullptr, log.c_str(), "HLSL -> SPIR-V Compile Error", MB_ICONERROR);
		return false;
	}
	return true;
}

/*
* 関数名　UpdateAndCompileShaders
* 引　数　なし
* 戻り値　なし
* 説　明　更新時刻が変わった HLSL だけをコンパイルし直して読み込む（ゲームでは何もしない）
*         初回は、既存のバイナリが HLSL より新しければコンパイルを省略する
*/
void ShaderManager::UpdateAndCompileShaders() {
	if (!m_cls_Device || !MainFrame::GetInstance()->IsEditorMode()) return;
	SettingManager* settings = SettingManager::GetInstance();
	const std::string hlslDir = settings->GetShaderFilePath();
	const std::string binDir = IsSpirvBackend() ? settings->GetSPIRVFilePath() : settings->GetCSOFilePath();
	std::error_code ec;
	for (auto it = std::filesystem::directory_iterator(FileUtil::PathToNative(hlslDir), ec);
		!ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
		if (!it->is_regular_file() || it->path().extension() != ".hlsl") continue;
		const std::string hlslPath = FileUtil::PathToUtf8(it->path());
		const std::string shaderName = it->path().stem().string();
		std::error_code ec2;
		const auto writeTime = it->last_write_time(ec2);
		auto found = m_hlslUpdateTimes.find(hlslPath);
		const bool firstScan = (found == m_hlslUpdateTimes.end());
		if (!firstScan && found->second == writeTime) continue;
		m_hlslUpdateTimes[hlslPath] = writeTime;

		const auto binTime = std::filesystem::last_write_time(FileUtil::PathToNative(binDir + shaderName + BinaryExtensionGet()), ec2);
		const bool upToDate = firstScan && !ec2 && binTime >= writeTime;
		if (!upToDate && !SourceCompile(hlslPath, shaderName)) continue;
		if (firstScan) continue;
		AssetManager::Instance()->CacheClear();   // 再コンパイル直後に古い内容を読まないように
		BinaryLoad(BinaryPrefixGet() + shaderName + BinaryExtensionGet(), shaderName);
	}
}

/*
* 関数名　BinariesLoad
* 引　数　なし
* 戻り値　なし
* 説　明　仮想パス shader/cso/ (Vulkan は shader/spv/) にある全バイナリを読み込む
*/
void ShaderManager::BinariesLoad()
{
	const std::string prefix = BinaryPrefixGet();
	for (auto& info : AssetManager::Instance()->FileListGet(prefix)) {
		const std::string file = info.path.substr(prefix.size());
		const size_t dot = file.find_last_of('.');
		if (dot == std::string::npos || file.find('/') != std::string::npos) continue;
		BinaryLoad(info.path, file.substr(0, dot));
	}
}

/*
* 関数名　BinaryLoad
* 引　数　virtualPath：バイナリの仮想パス　name：シェーダー名
* 戻り値　成功なら true
* 説　明　バイナリから RHI シェーダーを生成し、反射情報から定数バッファを用意する
*/
bool ShaderManager::BinaryLoad(const std::string& virtualPath, const std::string& name) {
	ShaderBindings::Stage stage{};
	if (!ShaderCompiler::StageFromName(name, stage)) return false;
	std::vector<uint8_t> code;
	if (!AssetManager::Instance()->FileRead(virtualPath, code) || code.empty()) return false;

	const bool vs = (stage == ShaderBindings::Stage::Vertex);
	RHI::ShaderRef shader = m_cls_Device->ShaderCreate(vs ? RHI::ShaderStage::Vertex : RHI::ShaderStage::Pixel,
		code.data(), code.size(), name.c_str());
	if (!shader) return false;
	// 置き換え前のシェーダーを保持しているコンポーネントがあるため、古いものは破棄せず退避する
	auto& slot = (vs ? m_vsShaders : m_psShaders)[name];
	if (slot) m_retiredShaders.push_back(slot);
	slot = shader;
	ReflectionBuild(vs ? ShaderStage::VS : ShaderStage::PS, name, shader->ReflectionGet());
	return true;
}

std::vector<std::string> ShaderManager::GetShaderList(const std::string& type) const {
	std::vector<std::string> result;
	if (type != "VS" && type != "PS") return result;
	const auto& map = (type == "VS") ? m_vsShaders : m_psShaders;
	for (const auto& kv : map) result.push_back(kv.first);
	std::sort(result.begin(), result.end());
	return result;
}

RHI::Shader* ShaderManager::GetVertexShader(const std::string& name) {
	auto it = m_vsShaders.find(name);
	return (it != m_vsShaders.end()) ? it->second.get() : nullptr;
}

RHI::Shader* ShaderManager::GetPixelShader(const std::string& name) {
	auto it = m_psShaders.find(name);
	return (it != m_psShaders.end()) ? it->second.get() : nullptr;
}

bool ShaderManager::CreateConstantBuffer(const std::string& key, UINT bufferSize) {
	if (!m_cls_Device) return false;
	RHI::BufferDesc desc;
	desc.i_Size = Align16(bufferSize);
	desc.i_BindFlags = RHI::Bind_ConstantBuffer;
	RHI::BufferRef buffer = m_cls_Device->BufferCreate(desc);
	if (!buffer) return false;
	m_constantBuffers[key].push_back(buffer);
	return true;
}

bool ShaderManager::WriteBuffer(const std::string& key, UINT slot, void* pData, UINT dataSize) {
	RHI::Buffer* buffer = GetConstantBuffer(key, slot);
	if (!buffer) return false;
	m_cls_Device->BufferUpdate(buffer, pData, dataSize);
	return true;
}

RHI::Buffer* ShaderManager::GetConstantBuffer(const std::string& key, UINT slot) {
	auto it = m_constantBuffers.find(key);
	if (it == m_constantBuffers.end() || slot >= it->second.size()) return nullptr;
	return it->second[slot].get();
}

/*
* 関数名　ReflectionBuild
* 引　数　stage / shaderName：対象　reflection：RHI の反射情報
* 戻り値　なし
* 説　明　定数バッファごとに GPU バッファと CPU 側の書き込み領域を用意する
*/
void ShaderManager::ReflectionBuild(ShaderStage stage, const std::string& shaderName, const RHI::ShaderReflection& reflection)
{
	ShaderReflectionData refData;
	for (auto& t : reflection.textures) {
		refData.texBindPointByName[t.name] = t.i_Slot;
		refData.textures.push_back({ t.name, t.i_Slot });
	}
	for (auto& cb : reflection.constantBuffers) {
		CBufferRuntime runtime;
		runtime.name = cb.name;
		runtime.bindPoint = cb.i_Slot;
		runtime.size = Align16(cb.i_Size);
		for (auto& v : cb.variables) runtime.varsByName[v.name] = { v.name, v.i_Offset, v.i_Size };
		RHI::BufferDesc desc;
		desc.i_Size = runtime.size;
		desc.i_BindFlags = RHI::Bind_ConstantBuffer;
		runtime.gpuBuffer = m_cls_Device->BufferCreate(desc);
		runtime.cpuData.assign(runtime.size, 0);
		refData.cbufIndexByName[runtime.name] = refData.cbuffers.size();
		refData.cbuffers.push_back(std::move(runtime));
	}
	(stage == ShaderStage::VS ? m_vsRef : m_psRef)[shaderName] = std::move(refData);
}

const ShaderManager::ShaderReflectionData* ShaderManager::GetReflection(
	ShaderStage stage, const std::string& shaderName) const {
	const auto& map = (stage == ShaderStage::VS) ? m_vsRef : m_psRef;
	auto it = map.find(shaderName);
	return (it != map.end()) ? &it->second : nullptr;
}

bool ShaderManager::SetCBufferVariable(ShaderStage stage, const std::string& shaderName,
	const std::string& cbName, const std::string& varName,
	const void* data, UINT size) {
	auto& refMap = (stage == ShaderStage::VS) ? m_vsRef : m_psRef;
	auto itRef = refMap.find(shaderName);
	if (itRef == refMap.end()) return false;
	auto itCB = itRef->second.cbufIndexByName.find(cbName);
	if (itCB == itRef->second.cbufIndexByName.end()) return false;
	auto& runtime = itRef->second.cbuffers[itCB->second];

	auto itVar = runtime.varsByName.find(varName);
	if (itVar == runtime.varsByName.end()) return false;
	const auto& vd = itVar->second;
	const UINT copySize = (size < vd.size) ? size : vd.size;
	if (vd.offset + copySize > runtime.cpuData.size()) return false;

	std::memcpy(runtime.cpuData.data() + vd.offset, data, copySize);
	runtime.dirty = true;
	return true;
}

bool ShaderManager::SetCBufferRaw(ShaderStage stage, const std::string& shaderName,
	const std::string& cbName, const void* data, UINT size) {
	auto& refMap = (stage == ShaderStage::VS) ? m_vsRef : m_psRef;
	auto itRef = refMap.find(shaderName);
	if (itRef == refMap.end()) return false;
	auto itCB = itRef->second.cbufIndexByName.find(cbName);
	if (itCB == itRef->second.cbufIndexByName.end()) return false;
	auto& runtime = itRef->second.cbuffers[itCB->second];

	if (size > runtime.size) return false;
	std::memcpy(runtime.cpuData.data(), data, size);
	runtime.dirty = true;
	return true;
}

/*
* 関数名　CommitAndBind
* 引　数　stage / shaderName：対象
* 戻り値　成功なら true
* 説　明　書き換えた定数バッファを GPU へ送り、反射情報のスロットにバインドする
*/
bool ShaderManager::CommitAndBind(ShaderStage stage, const std::string& shaderName) {
	auto& refMap = (stage == ShaderStage::VS) ? m_vsRef : m_psRef;
	auto itRef = refMap.find(shaderName);
	if (itRef == refMap.end() || !m_cls_Device) return false;
	for (auto& cb : itRef->second.cbuffers) {
		if (!cb.gpuBuffer) continue;
		if (cb.dirty) {
			m_cls_Device->BufferUpdate(cb.gpuBuffer.get(), cb.cpuData.data(), (uint32_t)cb.cpuData.size());
			cb.dirty = false;
		}
		m_cls_Device->ConstantBufferSet(StageConvert(stage), cb.bindPoint, cb.gpuBuffer.get());
	}
	return true;
}

bool ShaderManager::BindSRV(ShaderStage stage, const std::string& shaderName,
	const std::string& texVarName, RHI::Texture* texture) {
	auto* ref = GetReflection(stage, shaderName);
	if (!ref || !m_cls_Device) return false;
	auto it = ref->texBindPointByName.find(texVarName);
	if (it == ref->texBindPointByName.end()) return false;
	m_cls_Device->TextureSet(StageConvert(stage), it->second, texture);
	return true;
}
