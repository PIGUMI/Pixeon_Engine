#define NOMINMAX
#include "ImageRender.h"
#include "GameRenderTarget.h"
#include "SettingManager.h"


ImageRender::ImageRender() {
	_ComponentName = "ImageRender";
	_Type = ComponentManager::COMPONENT_TYPE::IMAGE;
}

ImageRender::~ImageRender() {
	UInit();
}

void ImageRender::UInit() {
	m_layout.reset();
	m_vs = nullptr;
	m_ps = nullptr;
	m_cbVS.reset();
	m_vb.reset();
	m_ib.reset();
}

void ImageRender::SaveToFile(std::ostream& out) {
	out << _LayerNumber << std::endl;
	out << m_textureName << std::endl;
	out << (int)m_mode << std::endl;
	out << m_vsName << std::endl;
	out << m_psName << std::endl;
	out << m_size2D.x << " " << m_size2D.y << std::endl;
	out << m_sizeWorld.x << " " << m_sizeWorld.y << std::endl;
	out << m_offset2D.x << " " << m_offset2D.y << std::endl;
	out << m_offset3D.x << " " << m_offset3D.y << " " << m_offset3D.z << std::endl;
	out << m_uvRect.x << " " << m_uvRect.y << " " << m_uvRect.z << " " << m_uvRect.w << std::endl;
	out << m_color.x << " " << m_color.y << " " << m_color.z << " " << m_color.w << std::endl;
	out << offset3DRot.x << " " << offset3DRot.y << " " << offset3DRot.z << std::endl;
	out << m_vertexOffsetUp << " " << m_vertexOffsetDown << " " << m_vertexOffsetLeft << " " << m_vertexOffsetRight << std::endl;
}

void ImageRender::LoadFromFile(std::istream& in) {
	int mode = 0;
	in >> _LayerNumber;
	in >> m_textureName;
	in >> mode; m_mode = (PlacementMode)mode;
	in >> m_vsName;
	in >> m_psName;
	in >> m_size2D.x >> m_size2D.y;
	in >> m_sizeWorld.x >> m_sizeWorld.y;
	in >> m_offset2D.x >> m_offset2D.y;
	in >> m_offset3D.x >> m_offset3D.y >> m_offset3D.z;
	in >> m_uvRect.x >> m_uvRect.y >> m_uvRect.z >> m_uvRect.w;
	in >> m_color.x >> m_color.y >> m_color.z >> m_color.w;
	in >> offset3DRot.x >> offset3DRot.y >> offset3DRot.z;

	// 頂点オフセットの読み込み（古いファイルとの互換性のため）
	if (in >> m_vertexOffsetUp >> m_vertexOffsetDown >> m_vertexOffsetLeft >> m_vertexOffsetRight) {
		// 読み込み成功
	}
	else {
		// 古いファイル形式の場合はデフォルト値を使用
		m_vertexOffsetUp = m_vertexOffsetDown = m_vertexOffsetLeft = m_vertexOffsetRight = 50.0f;
	}

	if (!m_textureName.empty()) {
		m_texture = ResourceService::Instance().GetTexture(m_textureName);
	}
}

void ImageRender::SetCamera(CameraComponent* ptr)
{
	cam = ptr;
}

void ImageRender::Init(AbstractObject* owner) {
	_Parent = owner;
	_ComponentName = "ImageRender";
	_Type = ComponentManager::COMPONENT_TYPE::IMAGE;
	m_textureName = "NULL";
	if (!m_textureName.empty()) {
		m_texture = ResourceService::Instance().GetTexture(m_textureName);
	}
	EnsureShaders(true);
	EnsureConstantBuffer();
	EnsureBuffers();
	m_ready = true;
	_LayerNumber = 0;
}

void ImageRender::InGameUpdate()
{
	Update();
}

void ImageRender::EditUpdate()
{
	Update();
}

void ImageRender::Update()
{
	AbstractScene* AbstractScene = _Parent ? _Parent->GetParentScene() : nullptr;
	if (AbstractScene)
	{
		cam = AbstractScene ? AbstractScene->GetMainCamera() : nullptr;
	}
}

void ImageRender::SetTextureName(const std::string& name) {
	m_textureName = name;
	m_texture = ResourceService::Instance().GetTexture(m_textureName);
}

bool ImageRender::EnsureShaders(bool forceRecreateLayout) {
	auto* sm = ShaderManager::GetInstance();
	RHI::Shader* vs = sm->GetVertexShader(m_vsName);
	RHI::Shader* ps = sm->GetPixelShader(m_psName);
	if (!vs || !ps) {
		sm->UpdateAndCompileShaders();
		vs = sm->GetVertexShader(m_vsName);
		ps = sm->GetPixelShader(m_psName);
		if (!vs || !ps) {
			OutputDebugStringA(("[ImageRender] Shader missing:  " + m_vsName + ", " + m_psName + "\n").c_str());
			return false;
		}
	}
	bool vsChanged = (m_vs != vs);
	bool needLayout = forceRecreateLayout || vsChanged || !m_layout;

	m_vs = vs;
	m_ps = ps;

	if (needLayout) RecreateInputLayout();
	return true;
}

void ImageRender::RecreateInputLayout() {
	m_layout.reset();
	EnsureInputLayout();
}

bool ImageRender::EnsureInputLayout() {
	if (m_layout) return true;
	if (!m_vs) return false;
	const RHI::InputElement desc[] = {
		{ "POSITION", 0, RHI::Format::R32G32B32_Float, 0, (uint32_t)offsetof(Vertex, pos) },
		{ "TEXCOORD", 0, RHI::Format::R32G32_Float,    0, (uint32_t)offsetof(Vertex, uv) },
	};
	m_layout = GraphicsSystem::GetInstance()->GetDevice()->InputLayoutCreate(desc, 2, m_vs);
	if (!m_layout) {
		OutputDebugStringA("[ImageRender] CreateInputLayout failed\n");
		return false;
	}
	return true;
}

bool ImageRender::EnsureConstantBuffer() {
	if (m_cbVS) return true;
	RHI::BufferDesc bd;
	bd.i_Size = sizeof(CBVS);
	bd.i_BindFlags = RHI::Bind_ConstantBuffer;
	m_cbVS = GraphicsSystem::GetInstance()->GetDevice()->BufferCreate(bd);
	if (!m_cbVS) {
		OutputDebugStringA("[ImageRender] Create CB failed\n");
		return false;
	}
	return true;
}

bool ImageRender::EnsureBuffers() {
	RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
	if (!m_vb) {
		RHI::BufferDesc bd;
		bd.i_Size = sizeof(Vertex) * 4;
		bd.i_BindFlags = RHI::Bind_VertexBuffer;
		bd.e_Usage = RHI::Usage::Dynamic;
		m_vb = dev->BufferCreate(bd);
		if (!m_vb) return false;
	}
	if (!m_ib) {
		const uint16_t idx[6] = { 0,1,2, 0,2,3 };
		RHI::BufferDesc bd;
		bd.i_Size = sizeof(idx);
		bd.i_BindFlags = RHI::Bind_IndexBuffer;
		bd.e_Usage = RHI::Usage::Immutable;
		m_ib = dev->BufferCreate(bd, idx);
		if (!m_ib) return false;
	}
	return true;
}

void ImageRender::UpdateVB(const Vertex v[4]) {
	GraphicsSystem::GetInstance()->GetDevice()->BufferUpdate(m_vb.get(), v, sizeof(Vertex) * 4);
}

void ImageRender::ApplyVertexOffsets(Vertex outV[4]) {
	// 頂点配置:  [0]=左上, [1]=右上, [2]=右下, [3]=左下

	// 中心点を計算
	float centerX = (outV[0].pos.x + outV[1].pos.x + outV[2].pos.x + outV[3].pos.x) * 0.25f;
	float centerY = (outV[0].pos.y + outV[1].pos.y + outV[2].pos.y + outV[3].pos.y) * 0.25f;

	// 各頂点の元の位置からの距離を計算
	float leftDist = centerX - outV[0].pos.x;    // 左側の距離（正の値）
	float rightDist = outV[1].pos.x - centerX;   // 右側の距離（正の値）
	float topDist = outV[0].pos.y - centerY;     // 上側の距離（正の値）
	float bottomDist = centerY - outV[3].pos.y;  // 下側の距離（正の値）

	// オフセット値を倍率に変換（50=1.0倍, 0=0倍, -50=-1.0倍）
	float leftScale = m_vertexOffsetLeft / 50.0f;
	float rightScale = m_vertexOffsetRight / 50.0f;
	float upScale = m_vertexOffsetUp / 50.0f;
	float downScale = m_vertexOffsetDown / 50.0f;

	// 左上 [0]
	outV[0].pos.x = centerX - (leftDist * leftScale);
	outV[0].pos.y = centerY + (topDist * upScale);

	// 右上 [1]
	outV[1].pos.x = centerX + (rightDist * rightScale);
	outV[1].pos.y = centerY + (topDist * upScale);

	// 右下 [2]
	outV[2].pos.x = centerX + (rightDist * rightScale);
	outV[2].pos.y = centerY - (bottomDist * downScale);

	// 左下 [3]
	outV[3].pos.x = centerX - (leftDist * leftScale);
	outV[3].pos.y = centerY - (bottomDist * downScale);
}

void ImageRender::UpdateVertices2D(Vertex outV[4], float& outZClip) {
	if (!cam) {
		float W = (float)GraphicsSystem::GetInstance()->GetBackBuffer()->GetWidth();
		float H = (float)GraphicsSystem::GetInstance()->GetBackBuffer()->GetHeight();

		float cx = W * 0.5f + m_offset2D.x;
		float cy = H * 0.5f + m_offset2D.y;

		float hw = m_size2D.x * 0.5f;
		float hh = m_size2D.y * 0.5f;
		auto toNDC = [&](float x, float y)->DirectX::XMFLOAT2 {
			float ndcX = (x / W) * 2.0f - 1.0f;
			float ndcY = 1.0f - (y / H) * 2.0f;
			return { ndcX, ndcY };
			};
		DirectX::XMFLOAT2 tl = toNDC(cx - hw, cy - hh);
		DirectX::XMFLOAT2 tr = toNDC(cx + hw, cy - hh);
		DirectX::XMFLOAT2 br = toNDC(cx + hw, cy + hh);
		DirectX::XMFLOAT2 bl = toNDC(cx - hw, cy + hh);
		outZClip = 0.0f;
		outV[0] = { {tl.x, tl.y, outZClip}, {m_uvRect.x, m_uvRect.y} };
		outV[1] = { {tr.x, tr.y, outZClip}, {m_uvRect.z, m_uvRect.y} };
		outV[2] = { {br.x, br.y, outZClip}, {m_uvRect.z, m_uvRect.w} };
		outV[3] = { {bl.x, bl.y, outZClip}, {m_uvRect.x, m_uvRect.w} };

		// 頂点オフセットを適用
		ApplyVertexOffsets(outV);
		return;
	}

	DirectX::XMMATRIX V = cam->GetView();
	DirectX::XMMATRIX P = cam->GetProjection();

	Transform t = _Parent->GetWorldTransform();
	DirectX::XMVECTOR posW = DirectX::XMVectorSet(t.position.x, t.position.y, t.position.z, 1.0f);

	DirectX::XMVECTOR clip = DirectX::XMVector4Transform(DirectX::XMVector4Transform(posW, V), P);
	DirectX::XMFLOAT4 clipF; DirectX::XMStoreFloat4(&clipF, clip);
	float W = (float)GraphicsSystem::GetInstance()->GetBackBuffer()->GetWidth();
	float H = (float)GraphicsSystem::GetInstance()->GetBackBuffer()->GetHeight();

	float ndcX = clipF.x / clipF.w;
	float ndcY = clipF.y / clipF.w;
	outZClip = clipF.z / clipF.w;

	float px = (ndcX * 0.5f + 0.5f) * W;
	float py = (-(ndcY) * 0.5f + 0.5f) * H;

	px += m_offset2D.x;
	py += m_offset2D.y;

	float hw = m_size2D.x * 0.5f;
	float hh = m_size2D.y * 0.5f;

	auto toNDC = [&](float x, float y)->DirectX::XMFLOAT2 {
		float ndcX2 = (x / W) * 2.0f - 1.0f;
		float ndcY2 = 1.0f - (y / H) * 2.0f;
		return { ndcX2, ndcY2 };
		};

	DirectX::XMFLOAT2 tl = toNDC(px - hw, py - hh);
	DirectX::XMFLOAT2 tr = toNDC(px + hw, py - hh);
	DirectX::XMFLOAT2 br = toNDC(px + hw, py + hh);
	DirectX::XMFLOAT2 bl = toNDC(px - hw, py + hh);

	outV[0] = { {tl.x, tl.y, outZClip}, {m_uvRect.x, m_uvRect.y} };
	outV[1] = { {tr.x, tr.y, outZClip}, {m_uvRect.z, m_uvRect.y} };
	outV[2] = { {br.x, br.y, outZClip}, {m_uvRect.z, m_uvRect.w} };
	outV[3] = { {bl.x, bl.y, outZClip}, {m_uvRect.x, m_uvRect.w} };

	// 頂点オフセットを適用
	ApplyVertexOffsets(outV);
}

void ImageRender::UpdateVerticesBillboard(Vertex outV[4]) {
	float hw = 0.5f;
	float hh = 0.5f;

	outV[0].pos = { -hw,  hh, 0.0f }; outV[0].uv = { m_uvRect.x, m_uvRect.y };
	outV[1].pos = { hw,  hh, 0.0f }; outV[1].uv = { m_uvRect.z, m_uvRect.y };
	outV[2].pos = { hw, -hh, 0.0f }; outV[2].uv = { m_uvRect.z, m_uvRect.w };
	outV[3].pos = { -hw, -hh, 0.0f }; outV[3].uv = { m_uvRect.x, m_uvRect.w };

	// 頂点オフセットを適用
	ApplyVertexOffsets(outV);
}

void ImageRender::UpdateVerticesWorld3D(Vertex outV[4]) {
	float hw = 0.5f;
	float hh = 0.5f;

	outV[0].pos = { -hw,  hh, 0.0f }; outV[0].uv = { m_uvRect.x, m_uvRect.y };
	outV[1].pos = { hw,  hh, 0.0f }; outV[1].uv = { m_uvRect.z, m_uvRect.y };
	outV[2].pos = { hw, -hh, 0.0f }; outV[2].uv = { m_uvRect.z, m_uvRect.w };
	outV[3].pos = { -hw, -hh, 0.0f }; outV[3].uv = { m_uvRect.x, m_uvRect.w };

	// 頂点オフセットを適用
	ApplyVertexOffsets(outV);
}

void ImageRender::UpdateVerticesUI(Vertex outV[4])
{
	if (!cam) {
		float dummyZ = 0.0f;
		UpdateVertices2D(outV, dummyZ);
		return;
	}

	float W = 1920.0f;
	float H = 1080.0f;

	Transform t = _Parent ? _Parent->GetWorldTransform() : Transform();
	float pixelWidth = m_sizeWorld.x * t.scale.x;
	float pixelHeight = m_sizeWorld.y * t.scale.y;

	float cx = W * 0.5f + m_offset2D.x;
	float cy = H * 0.5f + m_offset2D.y;

	float hw = pixelWidth * 0.5f;
	float hh = pixelHeight * 0.5f;

	auto toNDC = [&](float x, float y) -> DirectX::XMFLOAT2 {
		float ndcX = (x / W) * 2.0f - 1.0f;
		float ndcY = 1.0f - (y / H) * 2.0f;
		return { ndcX, ndcY };
		};

	DirectX::XMFLOAT2 tl = toNDC(cx - hw, cy - hh);
	DirectX::XMFLOAT2 tr = toNDC(cx + hw, cy - hh);
	DirectX::XMFLOAT2 br = toNDC(cx + hw, cy + hh);
	DirectX::XMFLOAT2 bl = toNDC(cx - hw, cy + hh);

	outV[0].pos = { tl.x, tl.y, 0.0f }; outV[0].uv = { m_uvRect.x, m_uvRect.y };
	outV[1].pos = { tr.x, tr.y, 0.0f }; outV[1].uv = { m_uvRect.z, m_uvRect.y };
	outV[2].pos = { br.x, br.y, 0.0f }; outV[2].uv = { m_uvRect.z, m_uvRect.w };
	outV[3].pos = { bl.x, bl.y, 0.0f }; outV[3].uv = { m_uvRect.x, m_uvRect.w };

	// 頂点オフセットを適用
	ApplyVertexOffsets(outV);
}

void ImageRender::Draw(int Layer) {
	if (Layer != _LayerNumber) return;
	if (!m_ready) return;
	GraphicsSystem* gs = GraphicsSystem::GetInstance();
	RHI::Device* dev = gs->GetDevice();
	if (!dev) return;

	if (!EnsureShaders(false)) return;
	if (!EnsureConstantBuffer()) return;
	if (!EnsureBuffers()) return;

	RHI::BlendState* prevBlend = dev->BlendStateGet();
	RHI::DepthStencilState* prevDepthStencil = dev->DepthStencilStateGet();
	gs->SetBlendMode(BLEND_ALPHA);
	gs->SetDepthMode(DEPTH_LESS_EQUAL);

	AbstractScene* AbstractScene = _Parent ? _Parent->GetParentScene() : nullptr;

	cam = AbstractScene ? AbstractScene->GetMainCamera() : nullptr;

	Vertex v[4]{};
	int mode2DFlag = 0;
	float zClip2D = 0.0f;

	if (m_mode == PlacementMode::Screen2D) {
		UpdateVertices2D(v, zClip2D);
		mode2DFlag = 1;
	}
	else if (m_mode == PlacementMode::Billboard) {
		UpdateVerticesBillboard(v);
		mode2DFlag = 0;
	}
	else if (m_mode == PlacementMode::UI) {
		UpdateVerticesUI(v);
		mode2DFlag = 1;
	}
	else {
		UpdateVerticesWorld3D(v);
		mode2DFlag = 2;
	}

	UpdateVB(v);

	CBVS cb{};

	if (_Parent) {
		Transform t = _Parent->GetWorldTransform();

		if (m_mode == PlacementMode::Billboard || m_mode == PlacementMode::UI) {
			DirectX::XMMATRIX S = DirectX::XMMatrixScaling(
				t.scale.x * m_sizeWorld.x,
				t.scale.y * m_sizeWorld.y,
				1.0f
			);

			DirectX::XMMATRIX R = DirectX::XMMatrixIdentity();
			DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(t.position.x, t.position.y, t.position.z);

			cb.World = DirectX::XMMatrixTranspose(S * R * T);
		}
		else {
			DirectX::XMMATRIX S = DirectX::XMMatrixScaling(
				t.scale.x * m_sizeWorld.x,
				t.scale.y * m_sizeWorld.y,
				1.0f
			);

			DirectX::XMMATRIX R;
			if (m_mode == PlacementMode::World3D) {
				R = DirectX::XMMatrixRotationRollPitchYaw(
					t.rotation.x + offset3DRot.x,
					t.rotation.y + offset3DRot.y,
					t.rotation.z + offset3DRot.z
				);
			}
			else {
				R = DirectX::XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
			}

			DirectX::XMVECTOR offset = DirectX::XMVector3Transform(
				DirectX::XMLoadFloat3(&m_offset3D),
				R
			);
			DirectX::XMFLOAT3 finalPos;
			DirectX::XMStoreFloat3(&finalPos,
				DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&t.position), offset)
			);

			DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(finalPos.x, finalPos.y, finalPos.z);

			cb.World = DirectX::XMMatrixTranspose(S * R * T);
		}
	}
	else {
		cb.World = DirectX::XMMatrixIdentity();
	}

	if (cam) {
		cb.View = DirectX::XMMatrixTranspose(cam->GetView());
		cb.Proj = DirectX::XMMatrixTranspose(cam->GetProjection());
	}
	else {
		cb.View = DirectX::XMMatrixIdentity();
		cb.Proj = DirectX::XMMatrixIdentity();
	}

	cb.Color = m_color;
	cb.mode2D = mode2DFlag;

	dev->BufferUpdate(m_cbVS.get(), &cb, sizeof(cb));

	RHI::Texture* texture = (m_texture && m_texture->texture) ? m_texture->texture.get() : gs->GetWhiteTexture();

	dev->VertexBufferSet(0, m_vb.get(), sizeof(Vertex), 0);
	dev->IndexBufferSet(m_ib.get(), RHI::IndexFormat::UInt16, 0);
	dev->InputLayoutSet(m_layout.get());
	dev->TopologySet(RHI::Topology::TriangleList);
	dev->ShaderSet(RHI::ShaderStage::Vertex, m_vs);
	dev->ShaderSet(RHI::ShaderStage::Pixel, m_ps);
	dev->ConstantBufferSet(RHI::ShaderStage::Vertex, 0, m_cbVS.get());
	dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 0, m_cbVS.get());
	dev->TextureSet(RHI::ShaderStage::Pixel, 0, texture);
	gs->SetSamplerState(SAMPLER_LINEAR_CLAMP);

	dev->IndexedDraw(6, 0, 0);

	dev->BlendStateSet(prevBlend);
	dev->DepthStencilStateSet(prevDepthStencil);
}

void ImageRender::DrawInspector() {
	auto SJ = [](const char* s)->std::string { return GUI::GetInstance()->ShiftJISToUTF8(s); };
	std::string title = _ComponentName + "##" + std::to_string((uintptr_t)this);
	if (!ImGui::CollapsingHeader(SJ(title.c_str()).c_str()))
		return;
	ImGui::Text("%s", SJ("レイヤー").c_str());
	ImGui::SameLine();
	ImGui::InputInt("##layer", &_LayerNumber);
	ImGui::Text("%s", SJ("テクスチャ名: ").c_str());
	ImGui::SameLine();
	ImGui::Text("%s", m_textureName.empty() ? "(none)" : m_textureName.c_str());
	ImGui::SameLine();
	if (ImGui::Button(SJ("選択... ").c_str())) {
		ImGui::OpenPopup("ImgTexSelectPopup");
	}
	if (ImGui::BeginPopupModal("ImgTexSelectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		static char filter[128] = "";
		ImGui::InputText(SJ("フィルタ").c_str(), filter, sizeof(filter));
		auto texList = AssetManager::Instance()->AssetListGet(AssetType::Texture);
		ImGui::BeginChild("ImgTexList", ImVec2(420, 260), true);
		for (int i = 0; i < (int)texList.size(); ++i) {
			const std::string& n = texList[i];
			if (filter[0] && n.find(filter) == std::string::npos) continue;
			if (ImGui::Selectable(n.c_str(), false)) {
				SetTextureName(n);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndChild();
		if (ImGui::Button(SJ("閉じる").c_str())) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (m_texture) {
		ImGui::Text("(%d x %d)", (int)m_texture->width, (int)m_texture->height);
	}

	std::string modeLabels[4] = { SJ("2D配置"), SJ("ビルボード"), SJ("3D配置"),SJ("UI") };
	int modeIdx = (int)m_mode;
	if (ImGui::BeginCombo(SJ("配置モード").c_str(), modeLabels[modeIdx].c_str())) {
		for (int i = 0; i < 4; ++i) {
			bool sel = (i == modeIdx);
			if (ImGui::Selectable(modeLabels[i].c_str(), sel)) {
				m_mode = (PlacementMode)i;
			}
			if (sel) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (m_mode == PlacementMode::UI) {
		ImGui::InputFloat2(SJ("2Dオフセット(px)").c_str(), (float*)&m_offset2D);
		ImGui::InputFloat2(SJ("サイズ(ワールド)").c_str(), (float*)&m_sizeWorld);
	}
	else if (m_mode == PlacementMode::Screen2D) {
		ImGui::InputFloat2(SJ("2Dオフセット(px)").c_str(), (float*)&m_offset2D);
		ImGui::InputFloat2(SJ("サイズ(px)").c_str(), (float*)&m_size2D);
	}
	else {
		ImGui::InputFloat3(SJ("3Dオフセット").c_str(), (float*)&m_offset3D);
		ImGui::InputFloat2(SJ("サイズ(ワールド)").c_str(), (float*)&m_sizeWorld);
		if (m_mode == PlacementMode::World3D)
		{
			DirectX::XMFLOAT3 TempRot;
			TempRot.x = DirectX::XMConvertToDegrees(offset3DRot.x);
			TempRot.y = DirectX::XMConvertToDegrees(offset3DRot.y);
			TempRot.z = DirectX::XMConvertToDegrees(offset3DRot.z);
			ImGui::InputFloat3(SJ("3D回転オフセット").c_str(), (float*)&TempRot);
			offset3DRot.x = DirectX::XMConvertToRadians(TempRot.x);
			offset3DRot.y = DirectX::XMConvertToRadians(TempRot.y);
			offset3DRot.z = DirectX::XMConvertToRadians(TempRot.z);
		}
	}

	ImGui::InputFloat4("UV(u0,v0,u1,v1)", (float*)&m_uvRect);
	m_uvRect.x = Clamp(m_uvRect.x, 0.0f, 1.0f);
	m_uvRect.y = Clamp(m_uvRect.y, 0.0f, 1.0f);
	m_uvRect.z = Clamp(m_uvRect.z, 0.0f, 1.0f);
	m_uvRect.w = Clamp(m_uvRect.w, 0.0f, 1.0f);

	ImGui::ColorEdit4(SJ("カラー").c_str(), (float*)&m_color);

	// 頂点オフセット設定
	if (ImGui::TreeNode(SJ("頂点オフセット設定").c_str())) {
		ImGui::Text(SJ("範囲:  -50～50 (50=最大サイズ, 0=中心)").c_str());

		float tempUp = m_vertexOffsetUp;
		if (ImGui::SliderFloat(SJ("上").c_str(), &tempUp, -50.0f, 50.0f)) {
			SetVertexOffsetUp(tempUp);
		}

		float tempDown = m_vertexOffsetDown;
		if (ImGui::SliderFloat(SJ("下").c_str(), &tempDown, -50.0f, 50.0f)) {
			SetVertexOffsetDown(tempDown);
		}

		float tempLeft = m_vertexOffsetLeft;
		if (ImGui::SliderFloat(SJ("左").c_str(), &tempLeft, -50.0f, 50.0f)) {
			SetVertexOffsetLeft(tempLeft);
		}

		float tempRight = m_vertexOffsetRight;
		if (ImGui::SliderFloat(SJ("右").c_str(), &tempRight, -50.0f, 50.0f)) {
			SetVertexOffsetRight(tempRight);
		}

		if (ImGui::Button(SJ("リセット").c_str())) {
			m_vertexOffsetUp = m_vertexOffsetDown = m_vertexOffsetLeft = m_vertexOffsetRight = 50.0f;
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode(SJ("シェーダ設定").c_str())) {
		auto* sm = ShaderManager::GetInstance();
		static std::vector<std::string> vsList;
		static std::vector<std::string> psList;
		if (ImGui::Button(SJ("リスト更新").c_str())) {
			vsList = sm->GetShaderList("VS");
			psList = sm->GetShaderList("PS");
		}
		if (vsList.empty()) vsList = sm->GetShaderList("VS");
		if (psList.empty()) psList = sm->GetShaderList("PS");

		if (ImGui::BeginCombo("VS", m_vsName.c_str())) {
			for (auto& n : vsList) {
				bool sel = (n == m_vsName);
				if (ImGui::Selectable(n.c_str(), sel)) {
					m_vsName = n;
					EnsureShaders(true);
				}
				if (sel) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (ImGui::BeginCombo("PS", m_psName.c_str())) {
			for (auto& n : psList) {
				bool sel = (n == m_psName);
				if (ImGui::Selectable(n.c_str(), sel)) {
					m_psName = n;
					EnsureShaders(false);
				}
				if (sel) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::TreePop();
	}
}
