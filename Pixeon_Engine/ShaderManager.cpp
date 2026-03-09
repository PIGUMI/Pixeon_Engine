#include <d3d11shader.h>
#include "ShaderManager.h"
#include "SettingManager.h"
#include <fstream>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

ShaderManager* ShaderManager::instance = nullptr;

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

void ShaderManager::Initialize(ID3D11Device* device) {
	m_device = device;
	UpdateAndCompileShaders();
}

void ShaderManager::Finalize() {
	for (auto& kv : m_vsShaders) if (kv.second) kv.second->Release();
	for (auto& kv : m_psShaders) if (kv.second) kv.second->Release();
	m_vsShaders.clear();
	m_psShaders.clear();

	m_vsBytecodes.clear();
	m_psBytecodes.clear();

	for (auto& kv : m_vsRef) {
		for (auto& cb : kv.second.cbuffers) if (cb.gpuBuffer) cb.gpuBuffer->Release();
	}
	for (auto& kv : m_psRef) {
		for (auto& cb : kv.second.cbuffers) if (cb.gpuBuffer) cb.gpuBuffer->Release();
	}
	m_vsRef.clear();
	m_psRef.clear();

	for (auto& kv : m_constantBuffers) {
		for (auto* buf : kv.second) if (buf) buf->Release();
	}
	m_constantBuffers.clear();
}

static UINT Align16(UINT v) { return (v + 15) & ~15u; }

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

bool ShaderManager::CompileHLSL(const std::string& hlslPath, const std::string& entry,
	const std::string& target, const std::string& csoPath) {
	ID3DBlob* code = nullptr;
	ID3DBlob* error = nullptr;
	HRESULT hr = D3DCompileFromFile(
		std::wstring(hlslPath.begin(), hlslPath.end()).c_str(),
		nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entry.c_str(), target.c_str(),
		0, 0, &code, &error);
	if (FAILED(hr)) {
		if (error) {
			MessageBoxA(nullptr, (char*)error->GetBufferPointer(), "HLSL Compile Error", MB_ICONERROR);
			error->Release();
		}
		if (code) code->Release();
		return false;
	}
	std::ofstream ofs(csoPath, std::ios::binary);
	ofs.write((char*)code->GetBufferPointer(), code->GetBufferSize());
	code->Release();
	return true;
}

void ShaderManager::UpdateAndCompileShaders() {
	std::string hlslDir = SettingManager::GetInstance()->GetShaderFilePath();
	std::string csoDir = SettingManager::GetInstance()->GetCSOFilePath();

	for (const auto& entry : std::filesystem::directory_iterator(hlslDir)) {
		std::string hlslPath = entry.path().string();
		std::string shaderName = entry.path().stem().string();

		WIN32_FILE_ATTRIBUTE_DATA fileInfo;
		GetFileAttributesExA(hlslPath.c_str(), GetFileExInfoStandard, &fileInfo);
		auto it = m_hlslUpdateTimes.find(hlslPath);
		if (it == m_hlslUpdateTimes.end() || CompareFileTime(&it->second, &fileInfo.ftLastWriteTime) < 0) {
			if (shaderName.find("VS_") != std::string::npos) {
				std::string csoPath = csoDir + shaderName + ".cso";
				if (CompileHLSL(hlslPath, "main", "vs_5_0", csoPath))
					LoadCSO(csoPath, "VS", shaderName);
			}
			else if (shaderName.find("PS_") != std::string::npos) {
				std::string csoPath = csoDir + shaderName + ".cso";
				if (CompileHLSL(hlslPath, "main", "ps_5_0", csoPath))
					LoadCSO(csoPath, "PS", shaderName);
			}
			m_hlslUpdateTimes[hlslPath] = fileInfo.ftLastWriteTime;
		}
	}
}

bool ShaderManager::LoadCSO(const std::string& csoPath, const std::string& type, const std::string& name) {
	std::ifstream ifs(csoPath, std::ios::binary | std::ios::ate);
	if (!ifs) return false;
	std::streamsize size = ifs.tellg();
	ifs.seekg(0, std::ios::beg);
	std::vector<char> buffer(size);
	if (!ifs.read(buffer.data(), size)) return false;

	HRESULT hr = S_OK;
	if (type == "VS") {
		ID3D11VertexShader* vs = nullptr;
		hr = m_device->CreateVertexShader(buffer.data(), size, nullptr, &vs);
		if (SUCCEEDED(hr)) {
			m_vsShaders[name] = vs;
			m_vsBytecodes[name] = buffer;
			ReflectShader(ShaderStage::VS, name, buffer.data(), buffer.size());
		}
	}
	else if (type == "PS") {
		ID3D11PixelShader* ps = nullptr;
		hr = m_device->CreatePixelShader(buffer.data(), size, nullptr, &ps);
		if (SUCCEEDED(hr)) {
			m_psShaders[name] = ps;
			m_psBytecodes[name] = buffer;
			ReflectShader(ShaderStage::PS, name, buffer.data(), buffer.size());
		}
	}
	return SUCCEEDED(hr);
}

std::vector<std::string> ShaderManager::GetShaderList(const std::string& type) const {
	std::vector<std::string> result;
	if (type == "VS") {
		for (const auto& kv : m_vsShaders) result.push_back(kv.first);
	}
	else if (type == "PS") {
		for (const auto& kv : m_psShaders) result.push_back(kv.first);
	}
	return result;
}

ID3D11VertexShader* ShaderManager::GetVertexShader(const std::string& name) {
	auto it = m_vsShaders.find(name);
	return (it != m_vsShaders.end()) ? it->second : nullptr;
}

ID3D11PixelShader* ShaderManager::GetPixelShader(const std::string& name) {
	auto it = m_psShaders.find(name);
	return (it != m_psShaders.end()) ? it->second : nullptr;
}

bool ShaderManager::CreateConstantBuffer(const std::string& key, UINT bufferSize) {
	if (!m_device) return false;
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = Align16(bufferSize);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	ID3D11Buffer* buffer = nullptr;
	HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &buffer);
	if (FAILED(hr)) return false;

	m_constantBuffers[key].push_back(buffer);
	return true;
}

bool ShaderManager::WriteBuffer(const std::string& key, UINT slot, void* pData, UINT dataSize) {
	auto it = m_constantBuffers.find(key);
	if (it == m_constantBuffers.end()) return false;
	if (slot >= it->second.size()) return false;

	ID3D11DeviceContext* pContext = nullptr;
	m_device->GetImmediateContext(&pContext);
	if (!pContext) return false;

	pContext->UpdateSubresource(it->second[slot], 0, nullptr, pData, 0, 0);
	pContext->Release();
	return true;
}

ID3D11Buffer* ShaderManager::GetConstantBuffer(const std::string& key, UINT slot) {
	auto it = m_constantBuffers.find(key);
	if (it == m_constantBuffers.end()) return nullptr;
	if (slot >= it->second.size()) return nullptr;
	return it->second[slot];
}

bool ShaderManager::GetVSBytecode(const std::string& name, const void** ppData, size_t* pSize) const {
	auto it = m_vsBytecodes.find(name);
	if (it == m_vsBytecodes.end()) return false;
	if (ppData) *ppData = it->second.data();
	if (pSize)  *pSize = it->second.size();
	return true;
}

bool ShaderManager::ReflectShader(ShaderStage stage, const std::string& shaderName,
	const void* bytecode, size_t size) {
	ID3D11ShaderReflection* refl = nullptr;
	HRESULT hr = D3DReflect(bytecode, size, __uuidof(ID3D11ShaderReflection), (void**)&refl);
	if (FAILED(hr) || !refl) return false;

	D3D11_SHADER_DESC sdesc = {};
	refl->GetDesc(&sdesc);

	ShaderReflectionData refData;

	std::unordered_map<std::string, UINT> bindPointByCBufferName;
	for (UINT r = 0; r < sdesc.BoundResources; ++r) {
		D3D11_SHADER_INPUT_BIND_DESC bindDesc = {};
		refl->GetResourceBindingDesc(r, &bindDesc);
		if (bindDesc.Type == D3D_SIT_CBUFFER) {
			bindPointByCBufferName[bindDesc.Name] = bindDesc.BindPoint;
		}
		else if (bindDesc.Type == D3D_SIT_TEXTURE) {
			TextureBindDesc td;
			td.name = bindDesc.Name ? bindDesc.Name : "";
			td.bindPoint = bindDesc.BindPoint;
			refData.texBindPointByName[td.name] = td.bindPoint;
			refData.textures.push_back(td);
		}
	}

	for (UINT i = 0; i < sdesc.ConstantBuffers; ++i) {
		ID3D11ShaderReflectionConstantBuffer* cb = refl->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC cbdesc = {};
		cb->GetDesc(&cbdesc);

		CBufferRuntime runtime;
		runtime.name = cbdesc.Name ? cbdesc.Name : "";
		runtime.size = Align16(cbdesc.Size);
		auto itBP = bindPointByCBufferName.find(runtime.name);
		runtime.bindPoint = (itBP != bindPointByCBufferName.end()) ? itBP->second : 0;

		for (UINT v = 0; v < cbdesc.Variables; ++v) {
			ID3D11ShaderReflectionVariable* var = cb->GetVariableByIndex(v);
			D3D11_SHADER_VARIABLE_DESC vdesc = {};
			var->GetDesc(&vdesc);

			VariableDesc vd;
			vd.name = vdesc.Name ? vdesc.Name : "";
			vd.offset = vdesc.StartOffset;
			vd.size = vdesc.Size;
			runtime.varsByName[vd.name] = vd;
		}

		D3D11_BUFFER_DESC bd = {};
		bd.ByteWidth = runtime.size;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		ID3D11Buffer* buf = nullptr;
		hr = m_device->CreateBuffer(&bd, nullptr, &buf);
		if (FAILED(hr)) { refl->Release(); return false; }

		runtime.gpuBuffer = buf;
		runtime.cpuData.resize(runtime.size, 0);
		runtime.dirty = false;

		refData.cbufIndexByName[runtime.name] = (UINT)refData.cbuffers.size();
		refData.cbuffers.push_back(std::move(runtime));
	}

	if (stage == ShaderStage::VS) {
		auto itOld = m_vsRef.find(shaderName);
		if (itOld != m_vsRef.end()) {
			for (auto& cb : itOld->second.cbuffers) if (cb.gpuBuffer) cb.gpuBuffer->Release();
		}
		m_vsRef[shaderName] = std::move(refData);
	}
	else {
		auto itOld = m_psRef.find(shaderName);
		if (itOld != m_psRef.end()) {
			for (auto& cb : itOld->second.cbuffers) if (cb.gpuBuffer) cb.gpuBuffer->Release();
		}
		m_psRef[shaderName] = std::move(refData);
	}

	refl->Release();
	return true;
}

const ShaderManager::ShaderReflectionData* ShaderManager::GetReflection(
	ShaderStage stage, const std::string& shaderName) const {
	if (stage == ShaderStage::VS) {
		auto it = m_vsRef.find(shaderName);
		return (it != m_vsRef.end()) ? &it->second : nullptr;
	}
	else {
		auto it = m_psRef.find(shaderName);
		return (it != m_psRef.end()) ? &it->second : nullptr;
	}
}

bool ShaderManager::SetCBufferVariable(ShaderStage stage, const std::string& shaderName,
	const std::string& cbName, const std::string& varName,
	const void* data, UINT size) {
	auto* ref = GetReflection(stage, shaderName);
	if (!ref) return false;
	auto itCB = ref->cbufIndexByName.find(cbName);
	if (itCB == ref->cbufIndexByName.end()) return false;

	auto& refMap = (stage == ShaderStage::VS) ? m_vsRef : m_psRef;
	auto& runtime = refMap[shaderName].cbuffers[itCB->second];

	auto itVar = runtime.varsByName.find(varName);
	if (itVar == runtime.varsByName.end()) return false;

	const auto& vd = itVar->second;
	const UINT copySize = (size < vd.size) ? size : vd.size;
	if (vd.offset + copySize > runtime.cpuData.size()) return false;

	memcpy(runtime.cpuData.data() + vd.offset, data, copySize);
	runtime.dirty = true;
	return true;
}

bool ShaderManager::SetCBufferRaw(ShaderStage stage, const std::string& shaderName,
	const std::string& cbName, const void* data, UINT size) {
	auto* ref = GetReflection(stage, shaderName);
	if (!ref) return false;
	auto itCB = ref->cbufIndexByName.find(cbName);
	if (itCB == ref->cbufIndexByName.end()) return false;

	auto& refMap = (stage == ShaderStage::VS) ? m_vsRef : m_psRef;
	auto& runtime = refMap[shaderName].cbuffers[itCB->second];

	if (size != runtime.size) return false;
	memcpy(runtime.cpuData.data(), data, size);
	runtime.dirty = true;
	return true;
}

bool ShaderManager::CommitAndBind(ShaderStage stage, const std::string& shaderName) {
	auto* ref = GetReflection(stage, shaderName);
	if (!ref) return false;

	ID3D11DeviceContext* ctx = nullptr;
	m_device->GetImmediateContext(&ctx);
	if (!ctx) return false;

	for (size_t i = 0; i < ref->cbuffers.size(); ++i) {
		auto& refMap = (stage == ShaderStage::VS) ? m_vsRef : m_psRef;
		auto& cb = refMap[shaderName].cbuffers[i];
		if (!cb.gpuBuffer) continue;

		if (cb.dirty) {
			ctx->UpdateSubresource(cb.gpuBuffer, 0, nullptr, cb.cpuData.data(), 0, 0);
			cb.dirty = false;
		}
		if (stage == ShaderStage::VS)
			ctx->VSSetConstantBuffers(cb.bindPoint, 1, &cb.gpuBuffer);
		else
			ctx->PSSetConstantBuffers(cb.bindPoint, 1, &cb.gpuBuffer);
	}
	ctx->Release();
	return true;
}

bool ShaderManager::BindSRV(ShaderStage stage, const std::string& shaderName,
	const std::string& texVarName, ID3D11ShaderResourceView* srv) {
	auto* ref = GetReflection(stage, shaderName);
	if (!ref) return false;
	auto it = ref->texBindPointByName.find(texVarName);
	if (it == ref->texBindPointByName.end()) return false;

	ID3D11DeviceContext* ctx = nullptr;
	m_device->GetImmediateContext(&ctx);
	if (!ctx) return false;

	UINT slot = it->second;
	if (stage == ShaderStage::VS)
		ctx->VSSetShaderResources(slot, 1, &srv);
	else
		ctx->PSSetShaderResources(slot, 1, &srv);

	ctx->Release();
	return true;
}