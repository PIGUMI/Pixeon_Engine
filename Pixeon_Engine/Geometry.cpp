#include "Geometry.h"
#include "ShaderManager.h"
#include "System.h"
#include "IMGUI/imgui.h"
#include "Scene.h"
#include "CameraComponent.h"

Geometry::Geometry() {}
Geometry::~Geometry() {
	if (m_vertexBuffer)  m_vertexBuffer->Release();
	if (m_inputLayout)   m_inputLayout->Release();
}

void Geometry::Init(Object* Prt) {
	_Parent = Prt;
	_ComponentName = "Geometry";
	_Type = ComponentManager::COMPONENT_TYPE::GEOMETRY;

	UpdateGridVertices();
	UpdateCubeVertices();
	UpdateVertexBuffer();
}

void Geometry::DrawInspector()
{
	std::string label = EditrGUI::GetInstance()->ShiftJISToUTF8(_ComponentName);
	std::string Ptr = std::to_string((uintptr_t)this);
	label += "###" + Ptr;

	if (ImGui::CollapsingHeader(EditrGUI::GetInstance()->ShiftJISToUTF8(label).c_str())) {
		if (ImGui::BeginTable("GeometryTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Grid Size");
			ImGui::TableSetColumnIndex(1);
			if (ImGui::DragFloat("##GridSize", &m_gridSize, 0.1f, 0.0f, 100.0f)) {
				m_needsUpdate = true;
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Grid Count");
			ImGui::TableSetColumnIndex(1);
			if (ImGui::DragInt("##GridCount", &m_gridCount, 1, 0, 100)) {
				m_needsUpdate = true;
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Grid Color");
			ImGui::TableSetColumnIndex(1); ImGui::ColorEdit4("##GridColor", reinterpret_cast<float*>(&m_gridColor));

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Cube Center");
			ImGui::TableSetColumnIndex(1);
			if (ImGui::DragFloat3("##CubeCenter", reinterpret_cast<float*>(&m_cubeCenter), 0.1f)) {
				m_needsUpdate = true;
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Cube Size");
			ImGui::TableSetColumnIndex(1);
			if (ImGui::DragFloat("##CubeSize", &m_cubeSize, 0.1f, 0.1f, 100.0f)) {
				m_needsUpdate = true;
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Cube Color");
			ImGui::TableSetColumnIndex(1); ImGui::ColorEdit4("##CubeColor", reinterpret_cast<float*>(&m_cubeColor));

			auto vsList = ShaderManager::GetInstance()->GetShaderList("VS");
			auto psList = ShaderManager::GetInstance()->GetShaderList("PS");
			static int vs_selected = 0;
			static int ps_selected = 0;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Vertex Shader");
			ImGui::TableSetColumnIndex(1);
			if (!vsList.empty()) {
				std::vector<const char*> vsNames;
				for (const auto& name : vsList) vsNames.push_back(name.c_str());
				if (ImGui::Combo("##VertexShader", &vs_selected, vsNames.data(), (int)vsNames.size())) {
					m_selectedVS = vsList[vs_selected];
				}
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Pixel Shader");
			ImGui::TableSetColumnIndex(1);
			if (!psList.empty()) {
				std::vector<const char*> psNames;
				for (const auto& name : psList) psNames.push_back(name.c_str());
				if (ImGui::Combo("##PixelShader", &ps_selected, psNames.data(), (int)psNames.size())) {
					m_selectedPS = psList[ps_selected];
				}
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text(EditrGUI::GetInstance()->ShiftJISToUTF8("設定を反映").c_str());
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Button("Apply Settings")) {
				m_needsUpdate = true;
				UpdateGridVertices();
				UpdateCubeVertices();
				UpdateVertexBuffer();

				m_vsShader = ShaderManager::GetInstance()->GetVertexShader(m_selectedVS);
				m_psShader = ShaderManager::GetInstance()->GetPixelShader(m_selectedPS);
				_selectNowVS = m_selectedVS;
				_selectNowPS = m_selectedPS;

				RebuildInputLayoutForCurrentVS();
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("Debug: Vertex Count");
			ImGui::TableSetColumnIndex(1); ImGui::Text("%d", (int)m_lineVertices.size());

			ImGui::EndTable();
		}
	}
}

void Geometry::UpdateGridVertices()
{
	if (m_gridCount == 0) {
		size_t cubeVertexCount = 24;
		if (m_lineVertices.size() > cubeVertexCount) {
			m_lineVertices.erase(m_lineVertices.begin(), m_lineVertices.end() - cubeVertexCount);
		}
		return;
	}

	std::vector<LineVertex> cubeVertices;
	if (m_lineVertices.size() >= 24) {
		cubeVertices.assign(m_lineVertices.end() - 24, m_lineVertices.end());
		m_lineVertices.erase(m_lineVertices.end() - 24, m_lineVertices.end());
	}

	m_lineVertices.clear();

	float half = m_gridSize * m_gridCount * 0.5f;

	for (int i = -m_gridCount; i <= m_gridCount; ++i) {
		float x = i * m_gridSize;
		m_lineVertices.push_back({ {x, 0.0f, -half}, m_gridColor });
		m_lineVertices.push_back({ {x, 0.0f,  half}, m_gridColor });
	}

	for (int i = -m_gridCount; i <= m_gridCount; ++i) {
		float z = i * m_gridSize;
		m_lineVertices.push_back({ {-half, 0.0f, z}, m_gridColor });
		m_lineVertices.push_back({ { half, 0.0f, z}, m_gridColor });
	}

	m_lineVertices.insert(m_lineVertices.end(), cubeVertices.begin(), cubeVertices.end());
}

void Geometry::UpdateCubeVertices()
{
	if (m_lineVertices.size() >= 24) {
		m_lineVertices.erase(m_lineVertices.end() - 24, m_lineVertices.end());
	}

	float half = m_cubeSize * 0.5f;
	DirectX::XMFLOAT3 v[8] = {
		{m_cubeCenter.x - half, m_cubeCenter.y - half, m_cubeCenter.z - half},
		{m_cubeCenter.x + half, m_cubeCenter.y - half, m_cubeCenter.z - half},
		{m_cubeCenter.x + half, m_cubeCenter.y - half, m_cubeCenter.z + half},
		{m_cubeCenter.x - half, m_cubeCenter.y - half, m_cubeCenter.z + half},
		{m_cubeCenter.x - half, m_cubeCenter.y + half, m_cubeCenter.z - half},
		{m_cubeCenter.x + half, m_cubeCenter.y + half, m_cubeCenter.z - half},
		{m_cubeCenter.x + half, m_cubeCenter.y + half, m_cubeCenter.z + half},
		{m_cubeCenter.x - half, m_cubeCenter.y + half, m_cubeCenter.z + half},
	};

	int edge[12][2] = {
		{0,1},{1,2},{2,3},{3,0},
		{4,5},{5,6},{6,7},{7,4},
		{0,4},{1,5},{2,6},{3,7}
	};

	for (int i = 0; i < 12; ++i) {
		m_lineVertices.push_back({ v[edge[i][0]], m_cubeColor });
		m_lineVertices.push_back({ v[edge[i][1]], m_cubeColor });
	}
}

void Geometry::UpdateVertexBuffer()
{
	if (m_vertexBuffer) {
		m_vertexBuffer->Release();
		m_vertexBuffer = nullptr;
	}
	if (m_lineVertices.empty()) return;

	auto* device = DirectX11::GetInstance()->GetDevice();
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = UINT(sizeof(LineVertex) * m_lineVertices.size());
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA srd{};
	srd.pSysMem = m_lineVertices.data();

	HRESULT hr = device->CreateBuffer(&bd, &srd, &m_vertexBuffer);
	if (FAILED(hr)) m_vertexBuffer = nullptr;
}

bool Geometry::RebuildInputLayoutForCurrentVS()
{
	if (m_inputLayout) { m_inputLayout->Release(); m_inputLayout = nullptr; }
	if (_selectNowVS.empty()) return false;

	const void* bc = nullptr;
	size_t      sz = 0;
	if (!ShaderManager::GetInstance()->GetVSBytecode(_selectNowVS, &bc, &sz)) return false;

	D3D11_INPUT_ELEMENT_DESC descs[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	HRESULT hr = DirectX11::GetInstance()->GetDevice()->CreateInputLayout(
		descs, _countof(descs), bc, sz, &m_inputLayout);
	return SUCCEEDED(hr);
}

void Geometry::Draw()
{
	auto* context = DirectX11::GetInstance()->GetContext();
	if (!context || !m_vertexBuffer) return;

	// 初回デフォルト
	if (!m_vsShader || !m_psShader) {
		auto vsList = ShaderManager::GetInstance()->GetShaderList("VS");
		auto psList = ShaderManager::GetInstance()->GetShaderList("PS");
		if (!vsList.empty() && !psList.empty()) {
			m_vsShader = ShaderManager::GetInstance()->GetVertexShader(vsList[0]);
			m_psShader = ShaderManager::GetInstance()->GetPixelShader(psList[0]);
			_selectNowVS = vsList[0];
			_selectNowPS = psList[0];
			RebuildInputLayoutForCurrentVS();
		}
	}
	if (!m_vsShader || !m_psShader || !m_inputLayout) return;

	context->IASetInputLayout(m_inputLayout);
	context->VSSetShader(m_vsShader, nullptr, 0);
	context->PSSetShader(m_psShader, nullptr, 0);

	// カメラ
	CameraComponent* camera = nullptr;
	if (_Parent && _Parent->GetParentScene())
		camera = _Parent->GetParentScene()->GetMainCamera();
	if (!camera) return;

	// VSのCameraCBが存在すれば書き込む（names by template）
	{
		// 行列はCPU側でtranspose済み（GetViewMatrix(true)など）
		DirectX::XMFLOAT4X4 world;
		DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()));

		DirectX::XMFLOAT4X4 view = camera->GetViewMatrix(true);
		DirectX::XMFLOAT4X4 proj = camera->GetProjectionMatrix(true);

		auto* sm = ShaderManager::GetInstance();

		// VS 側 cbuffer へ書き込み（反射ベース API を想定）
		sm->SetCBufferVariable(ShaderStage::VS, _selectNowVS, "CameraCB", "world", &world, sizeof(world));
		sm->SetCBufferVariable(ShaderStage::VS, _selectNowVS, "CameraCB", "view", &view, sizeof(view));
		sm->SetCBufferVariable(ShaderStage::VS, _selectNowVS, "CameraCB", "proj", &proj, sizeof(proj));

		// 変更をGPUに反映＆バインド
		sm->CommitAndBind(ShaderStage::VS, _selectNowVS);
	}

	// PSのMaterialCB(LineColor)があれば書き込む
	{
		ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::PS, _selectNowPS, "MaterialCB", "LineColor", &m_cubeColor, sizeof(m_cubeColor));
		ShaderManager::GetInstance()->CommitAndBind(ShaderStage::PS, _selectNowPS);
	}

	// 頂点バッファ設定
	UINT stride = sizeof(LineVertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	context->Draw((UINT)m_lineVertices.size(), 0);
}

// Geometryのセーブ
void Geometry::SaveToFile(std::ostream& out) {
	out << m_gridSize << " " << m_gridCount << " "
		<< m_gridColor.x << " " << m_gridColor.y << " " << m_gridColor.z << " " << m_gridColor.w << " "
		<< m_cubeCenter.x << " " << m_cubeCenter.y << " " << m_cubeCenter.z << " "
		<< m_cubeSize << " "
		<< m_cubeColor.x << " " << m_cubeColor.y << " " << m_cubeColor.z << " " << m_cubeColor.w << " "
		<< m_selectedVS << " "
		<< m_selectedPS << "\n";
}
// Geometryのロード
void Geometry::LoadFromFile(std::istream& in) {
	in >> m_gridSize >> m_gridCount
		>> m_gridColor.x >> m_gridColor.y >> m_gridColor.z >> m_gridColor.w
		>> m_cubeCenter.x >> m_cubeCenter.y >> m_cubeCenter.z
		>> m_cubeSize
		>> m_cubeColor.x >> m_cubeColor.y >> m_cubeColor.z >> m_cubeColor.w
		>> m_selectedVS
		>> m_selectedPS;
	m_needsUpdate = true;
	UpdateGridVertices();
	UpdateCubeVertices();
	UpdateVertexBuffer();
	m_vsShader = ShaderManager::GetInstance()->GetVertexShader(m_selectedVS);
	m_psShader = ShaderManager::GetInstance()->GetPixelShader(m_selectedPS);
	_selectNowVS = m_selectedVS;
	_selectNowPS = m_selectedPS;
	RebuildInputLayoutForCurrentVS();
}