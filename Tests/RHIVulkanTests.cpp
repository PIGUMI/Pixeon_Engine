/*
* ファイル名　RHIVulkanTests
* 説　　　明　RHI の Vulkan バックエンドを実際に描画して検証するテスト
*             GPU が無い環境でも Mesa の lavapipe (CPU 実装) で動く。検証レイヤがあれば有効にし、
*             警告・エラーが 1 件でも出たら失敗にする
*
*   検証すること
*     - クリア・読み戻し
*     - エンジンの SPIR-V シェーダー（HLSL から変換）での描画
*       行列のメモリ配置が D3D と一致するか、Y 軸の向き、表裏判定（カリング）
*     - テクスチャの UV 原点、レンダーターゲット → シェーダー入力の切り替え、インデックス描画
*     - 深度テスト、複数フレームの連続描画とリソースの遅延破棄
*     - ImGui の描画
*/
#include "TestFramework.h"
#include "RHI/RHI.h"
#include "IMGUI/imgui.h"
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifndef PIXEON_SHADER_DIR
#define PIXEON_SHADER_DIR "Exe/SceneRoot/Shader/spv"
#endif

namespace {
	constexpr uint32_t W = 64, H = 64;
	std::vector<std::string> g_ValidationMessages;

	void LogCapture(const char* message)
	{
		std::printf("  [RHI] %s\n", message);
		// DebugCallback 経由のメッセージ（検証レイヤの警告・エラー）だけ数える
		if (std::strncmp(message, "Vulkan: ", 8) == 0 && std::strstr(message, "Validation") != nullptr) g_ValidationMessages.push_back(message);
	}

	std::unique_ptr<RHI::Device> DeviceMake()
	{
		RHI::LogCallbackSet(LogCapture);
		RHI::DeviceCreateInfo info;
		info.i_Width = W;
		info.i_Height = H;
		info.b_Debug = true;
		std::string error;
		auto dev = RHI::DeviceCreate(RHI::Backend::Vulkan, info, &error);
		if (!dev) std::printf("  デバイス作成失敗: %s\n", error.c_str());
		return dev;
	}

	RHI::ShaderRef ShaderLoad(RHI::Device* dev, const char* name)
	{
		const std::string path = std::string(PIXEON_SHADER_DIR) + "/" + name + ".spv";
		std::ifstream f(path, std::ios::binary);
		std::vector<char> code((std::istreambuf_iterator<char>(f)), {});
		if (code.empty()) { std::printf("  シェーダーが読めません: %s\n", path.c_str()); return nullptr; }
		const RHI::ShaderStage stage = (name[0] == 'V') ? RHI::ShaderStage::Vertex : RHI::ShaderStage::Pixel;
		return dev->ShaderCreate(stage, code.data(), code.size(), name);
	}

	struct Pixel { uint8_t r, g, b, a; };
	struct Image {
		std::vector<uint8_t> data;
		uint32_t pitch = 0;
		Pixel At(uint32_t x, uint32_t y) const
		{
			const uint8_t* p = data.data() + (size_t)y * pitch + (size_t)x * 4;
			return { p[0], p[1], p[2], p[3] };
		}
	};
	Image Read(RHI::Device* dev, RHI::Texture* tex)
	{
		Image img;
		if (!dev->TextureRead(tex, img.data, img.pitch)) img.data.assign((size_t)W * H * 4, 0), img.pitch = W * 4;
		return img;
	}
	bool Near(Pixel p, uint8_t r, uint8_t g, uint8_t b)
	{
		auto d = [](uint8_t a, uint8_t b) { return a > b ? a - b : b - a; };
		return d(p.r, r) <= 2 && d(p.g, g) <= 2 && d(p.b, b) <= 2;
	}
	void PixelPrint(const char* label, Pixel p) { std::printf("  %s = (%u, %u, %u, %u)\n", label, p.r, p.g, p.b, p.a); }

	// HLSL の既定 (column_major) で行列 M を定数バッファに置いたときのメモリ配置
	void MatrixStore(float out[16], const float m[4][4])
	{
		for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) out[c * 4 + r] = m[r][c];
	}

	void BackBufferBind(RHI::Device* dev)
	{
		RHI::Texture* bb = dev->BackBufferGet();
		dev->RenderTargetsSet(1, &bb, nullptr);
		RHI::Viewport vp;
		vp.f_Width = (float)bb->GetWidth();
		vp.f_Height = (float)bb->GetHeight();
		dev->ViewportSet(vp);
	}

	// VS_ImgQuad / PS_ImgQuad の定数バッファ
	struct ImgQuadCB {
		float view[16];
		float proj[16];
		float color[4];
		int32_t mode2D;
		float pad[3];
	};
	struct QuadVertex { float x, y, z, u, v; };

	struct QuadPipeline {
		RHI::ShaderRef vs, ps;
		RHI::InputLayoutRef layout;
		RHI::BufferRef cb, vb, ib;
		RHI::SamplerRef point;
		bool Init(RHI::Device* dev)
		{
			vs = ShaderLoad(dev, "VS_ImgQuad");
			ps = ShaderLoad(dev, "PS_ImgQuad");
			if (!vs || !ps) return false;
			const RHI::InputElement elements[] = {
				{ "POSITION", 0, RHI::Format::R32G32B32_Float, 0, 0 },
				{ "TEXCOORD", 0, RHI::Format::R32G32_Float, 0, 12 },
			};
			layout = dev->InputLayoutCreate(elements, 2, vs.get());
			RHI::BufferDesc cd;
			cd.i_Size = sizeof(ImgQuadCB);
			cd.i_BindFlags = RHI::Bind_ConstantBuffer;
			cb = dev->BufferCreate(cd, nullptr);
			RHI::BufferDesc vd;
			vd.i_Size = sizeof(QuadVertex) * 4;
			vd.i_BindFlags = RHI::Bind_VertexBuffer;
			vb = dev->BufferCreate(vd, nullptr);
			const uint16_t indices[] = { 0, 1, 2, 2, 1, 3 };
			RHI::BufferDesc id;
			id.i_Size = sizeof(indices);
			id.i_BindFlags = RHI::Bind_IndexBuffer;
			id.e_Usage = RHI::Usage::Immutable;
			ib = dev->BufferCreate(id, indices);
			RHI::SamplerDesc sd;
			sd.e_Filter = RHI::Filter::Point;
			sd.e_AddressU = sd.e_AddressV = sd.e_AddressW = RHI::AddressMode::Clamp;
			point = dev->SamplerCreate(sd);
			return layout && cb && vb && ib && point;
		}
		// NDC の矩形 (l, t)-(r, b) に深度 z で texture を描く
		void Draw(RHI::Device* dev, RHI::Texture* texture, float l, float t, float r, float b, float z, const float color[4])
		{
			const QuadVertex v[4] = { { l, t, z, 0, 0 }, { r, t, z, 1, 0 }, { l, b, z, 0, 1 }, { r, b, z, 1, 1 } };
			dev->BufferUpdate(vb.get(), v, sizeof(v));
			ImgQuadCB c = {};
			std::memcpy(c.color, color, sizeof(c.color));
			c.mode2D = 1;
			dev->BufferUpdate(cb.get(), &c, sizeof(c));
			dev->ShaderSet(RHI::ShaderStage::Vertex, vs.get());
			dev->ShaderSet(RHI::ShaderStage::Pixel, ps.get());
			dev->InputLayoutSet(layout.get());
			dev->TopologySet(RHI::Topology::TriangleList);
			dev->VertexBufferSet(0, vb.get(), sizeof(QuadVertex));
			dev->IndexBufferSet(ib.get(), RHI::IndexFormat::UInt16);
			dev->ConstantBufferSet(RHI::ShaderStage::Vertex, 0, cb.get());
			dev->TextureSet(RHI::ShaderStage::Pixel, 0, texture);
			dev->SamplerSet(RHI::ShaderStage::Pixel, 0, point.get());
			dev->IndexedDraw(6);
		}
	};

	const float WHITE[4] = { 1, 1, 1, 1 };
}

PIX_TEST(Vulkan_ClearAndReadback)
{
	g_ValidationMessages.clear();
	auto dev = DeviceMake();
	PIX_CHECK(dev != nullptr);
	if (!dev) return;
	std::printf("  adapter: %s\n", dev->AdapterNameGet());
	const float red[4] = { 1, 0, 0, 1 };
	dev->RenderTargetClear(dev->BackBufferGet(), red);
	Image img = Read(dev.get(), dev->BackBufferGet());
	PIX_CHECK(Near(img.At(0, 0), 255, 0, 0));
	PIX_CHECK(Near(img.At(W - 1, H - 1), 255, 0, 0));
	dev->Present();
	PIX_CHECK(g_ValidationMessages.empty());
}

/*
* エンジンの VS_Line / PS_Line で三角形を描き、
*   - 行列（平行移動）が D3D と同じ解釈になるか
*   - +Y が画面の上か
*   - 時計回りが表 (D3D の既定) でカリングされるか
* を確かめる
*/
PIX_TEST(Vulkan_EngineShader_MatrixOrientationCulling)
{
	g_ValidationMessages.clear();
	auto dev = DeviceMake();
	if (!dev) { PIX_CHECK(false); return; }
	auto vs = ShaderLoad(dev.get(), "VS_Line");
	auto ps = ShaderLoad(dev.get(), "PS_Line");
	PIX_CHECK(vs && ps);
	if (!vs || !ps) return;

	const RHI::InputElement elements[] = {
		{ "POSITION", 0, RHI::Format::R32G32B32_Float, 0, 0 },
		{ "COLOR", 0, RHI::Format::R32G32B32A32_Float, 0, 12 },
	};
	auto layout = dev->InputLayoutCreate(elements, 2, vs.get());
	PIX_CHECK(layout != nullptr);

	struct Vertex { float x, y, z, r, g, b, a; };
	RHI::BufferDesc vd;
	vd.i_Size = sizeof(Vertex) * 3;
	vd.i_BindFlags = RHI::Bind_VertexBuffer;
	vd.e_Usage = RHI::Usage::Dynamic;
	auto vb = dev->BufferCreate(vd, nullptr);

	// world = 平行移動 (0.25, 0, 0)（D3D の行ベクトル形式: 4 行目に移動量）
	const float world[4][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0.25f, 0, 0, 1 } };
	const float ident[4][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
	float cb_data[48];
	MatrixStore(cb_data, world);
	MatrixStore(cb_data + 16, ident);
	MatrixStore(cb_data + 32, ident);
	RHI::BufferDesc cd;
	cd.i_Size = sizeof(cb_data);
	cd.i_BindFlags = RHI::Bind_ConstantBuffer;
	auto cb = dev->BufferCreate(cd, cb_data);

	RHI::RasterizerDesc rd;   // 既定: 裏面カリング・時計回りが表
	auto raster = dev->RasterizerStateCreate(rd);

	auto draw = [&](bool clockwise) {
		const float clear[4] = { 0, 0, 0, 1 };
		dev->RenderTargetClear(dev->BackBufferGet(), clear);
		// 画面上で時計回り: 左下 → 上 → 右下
		Vertex v[3] = { { -0.5f, -0.5f, 0, 0, 1, 0, 1 }, { 0, 0.5f, 0, 0, 1, 0, 1 }, { 0.5f, -0.5f, 0, 0, 1, 0, 1 } };
		if (!clockwise) std::swap(v[1], v[2]);
		std::memcpy(dev->BufferMap(vb.get()), v, sizeof(v));
		dev->BufferUnmap(vb.get());
		BackBufferBind(dev.get());
		dev->RasterizerStateSet(raster.get());
		dev->ShaderSet(RHI::ShaderStage::Vertex, vs.get());
		dev->ShaderSet(RHI::ShaderStage::Pixel, ps.get());
		dev->InputLayoutSet(layout.get());
		dev->TopologySet(RHI::Topology::TriangleList);
		dev->VertexBufferSet(0, vb.get(), sizeof(Vertex));
		dev->ConstantBufferSet(RHI::ShaderStage::Vertex, 0, cb.get());
		dev->Draw(3);
		return Read(dev.get(), dev->BackBufferGet());
	};

	Image img = draw(true);
	PixelPrint("center(40,37)", img.At(40, 37));
	PIX_CHECK(Near(img.At(40, 37), 0, 255, 0));   // 移動後の三角形の中心
	PIX_CHECK(Near(img.At(24, 37), 0, 0, 0));     // 移動していなければ塗られる位置
	PIX_CHECK(Near(img.At(40, 20), 0, 255, 0));   // 頂点（上）付近 → +Y が上
	PIX_CHECK(Near(img.At(40, 50), 0, 0, 0));     // 底辺より下

	img = draw(false);
	PixelPrint("culled center", img.At(40, 37));
	PIX_CHECK(Near(img.At(40, 37), 0, 0, 0));     // 反時計回りは裏面としてカリング
	dev->Present();
	PIX_CHECK(g_ValidationMessages.empty());
}

/*
* テクスチャの UV 原点（左上）、点サンプリング、レンダーターゲットをシェーダー入力として使う切り替え
*/
PIX_TEST(Vulkan_TextureUvAndRenderTargetSampling)
{
	g_ValidationMessages.clear();
	auto dev = DeviceMake();
	if (!dev) { PIX_CHECK(false); return; }
	QuadPipeline quad;
	PIX_CHECK(quad.Init(dev.get()));

	// 2x2: 上段 赤・緑、下段 青・白
	const uint32_t texels[4] = { 0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u, 0xFFFFFFFFu };
	RHI::TextureDesc td;
	td.i_Width = td.i_Height = 2;
	RHI::SubresourceData init{ texels, 8, 16 };
	auto checker = dev->TextureCreate(td, &init);
	PIX_CHECK(checker != nullptr);

	BackBufferBind(dev.get());
	quad.Draw(dev.get(), checker.get(), -1, 1, 1, -1, 0.5f, WHITE);
	Image img = Read(dev.get(), dev->BackBufferGet());
	PIX_CHECK(Near(img.At(8, 8), 255, 0, 0));
	PIX_CHECK(Near(img.At(56, 8), 0, 255, 0));
	PIX_CHECK(Near(img.At(8, 56), 0, 0, 255));
	PIX_CHECK(Near(img.At(56, 56), 255, 255, 255));

	// オフスクリーンに描いてから、それを入力にして画面の左半分へ描く
	RHI::TextureDesc rt;
	rt.i_Width = rt.i_Height = 32;
	rt.i_BindFlags = RHI::Bind_RenderTarget | RHI::Bind_ShaderResource;
	auto offscreen = dev->TextureCreate(rt, nullptr);
	const float blue[4] = { 0, 0, 1, 1 };
	dev->RenderTargetClear(offscreen.get(), blue);
	RHI::Texture* target = offscreen.get();
	dev->RenderTargetsSet(1, &target, nullptr);
	RHI::Viewport vp;
	vp.f_Width = vp.f_Height = 32;
	dev->ViewportSet(vp);
	quad.Draw(dev.get(), checker.get(), -1, 1, 0, 0, 0.5f, WHITE);   // 左上 1/4 にチェッカー

	const float gray[4] = { 0.5f, 0.5f, 0.5f, 1 };
	dev->RenderTargetClear(dev->BackBufferGet(), gray);
	BackBufferBind(dev.get());
	quad.Draw(dev.get(), offscreen.get(), -1, 1, 0, -1, 0.5f, WHITE);
	img = Read(dev.get(), dev->BackBufferGet());
	PixelPrint("left-bottom", img.At(8, 56));
	PIX_CHECK(Near(img.At(8, 56), 0, 0, 255));      // オフスクリーンの青
	PIX_CHECK(Near(img.At(2, 2), 255, 0, 0));       // オフスクリーン左上のチェッカー（赤）
	PIX_CHECK(Near(img.At(56, 32), 128, 128, 128)); // 右半分はクリア色
	dev->Present();
	PIX_CHECK(g_ValidationMessages.empty());
}

PIX_TEST(Vulkan_DepthTest)
{
	g_ValidationMessages.clear();
	auto dev = DeviceMake();
	if (!dev) { PIX_CHECK(false); return; }
	QuadPipeline quad;
	PIX_CHECK(quad.Init(dev.get()));
	const uint32_t white = 0xFFFFFFFFu;
	RHI::TextureDesc td;
	RHI::SubresourceData init{ &white, 4, 4 };
	auto tex = dev->TextureCreate(td, &init);

	for (RHI::Format depth_format : { RHI::Format::D32_Float, RHI::Format::D24_UNorm_S8_UInt }) {
		RHI::TextureDesc dd;
		dd.i_Width = W;
		dd.i_Height = H;
		dd.e_Format = depth_format;
		dd.i_BindFlags = RHI::Bind_DepthStencil | RHI::Bind_ShaderResource;
		auto depth = dev->TextureCreate(dd, nullptr);
		PIX_CHECK(depth != nullptr);
		if (!depth) continue;
		const float black[4] = { 0, 0, 0, 1 };
		dev->RenderTargetClear(dev->BackBufferGet(), black);
		dev->DepthStencilClear(depth.get(), true, true, 1.0f, 0);
		RHI::Texture* bb = dev->BackBufferGet();
		dev->RenderTargetsSet(1, &bb, depth.get());
		RHI::Viewport vp;
		vp.f_Width = (float)W;
		vp.f_Height = (float)H;
		dev->ViewportSet(vp);
		dev->DepthStencilStateSet(nullptr);   // 既定: LESS + 書き込み
		const float red[4] = { 1, 0, 0, 1 }, green[4] = { 0, 1, 0, 1 };
		quad.Draw(dev.get(), tex.get(), -0.5f, 0.5f, 0.5f, -0.5f, 0.3f, red);    // 手前
		quad.Draw(dev.get(), tex.get(), -1, 1, 1, -1, 0.6f, green);               // 奥（中央は隠れる）
		Image img = Read(dev.get(), bb);
		PixelPrint("center", img.At(32, 32));
		PIX_CHECK(Near(img.At(32, 32), 255, 0, 0));
		PIX_CHECK(Near(img.At(4, 4), 0, 255, 0));
		// 深度バッファをシェーダーで読めるか（描画先から外してから入力にする）
		dev->RenderTargetsSet(1, &bb, nullptr);
		quad.Draw(dev.get(), depth.get(), -1, 1, 1, -1, 0.0f, WHITE);
		img = Read(dev.get(), bb);
		PixelPrint("depth as texture (center)", img.At(32, 32));
		PIX_CHECK(img.At(32, 32).r > 60 && img.At(32, 32).r < 95);   // 0.3 → 約 77
	}
	dev->Present();
	PIX_CHECK(g_ValidationMessages.empty());
}

/*
* 複数フレームにわたって定数バッファの更新・テクスチャの生成と破棄を繰り返す
* （フレームリング・アップロードページの再利用・遅延破棄の検証）
*/
PIX_TEST(Vulkan_MultiFrameResourceChurn)
{
	g_ValidationMessages.clear();
	auto dev = DeviceMake();
	if (!dev) { PIX_CHECK(false); return; }
	QuadPipeline quad;
	PIX_CHECK(quad.Init(dev.get()));
	RHI::TextureRef keep_after_device;
	for (int frame = 0; frame < 12; ++frame) {
		const uint8_t level = (uint8_t)(frame * 20);
		const uint32_t texel = 0xFF000000u | level;
		RHI::TextureDesc td;
		RHI::SubresourceData init{ &texel, 4, 4 };
		auto tex = dev->TextureCreate(td, &init);
		BackBufferBind(dev.get());
		// 1 フレーム内で同じ定数バッファを何度も書き換える（D3D11 の UpdateSubresource と同じ結果になること）
		for (int i = 0; i < 4; ++i) {
			const float c[4] = { 1, 1, 1, 1 };
			const float x = -1.0f + i * 0.5f;
			quad.Draw(dev.get(), tex.get(), x, 1, x + 0.5f, -1, 0.5f, c);
		}
		if (frame == 11) {
			Image img = Read(dev.get(), dev->BackBufferGet());
			PIX_CHECK(Near(img.At(4, 32), level, 0, 0));
			PIX_CHECK(Near(img.At(60, 32), level, 0, 0));
			keep_after_device = tex;
		}
		dev->Present();
		PIX_CHECK(dev->FrameIndexGet() == (uint64_t)frame + 1);
	}
	quad = QuadPipeline{};
	dev.reset();
	// デバイスを破棄した後にリソースを解放しても安全なこと
	keep_after_device.reset();
	PIX_CHECK(g_ValidationMessages.empty());
}

PIX_TEST(Vulkan_ImGui)
{
	g_ValidationMessages.clear();
	auto dev = DeviceMake();
	if (!dev) { PIX_CHECK(false); return; }
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)W, (float)H);
	io.IniFilename = nullptr;
	{
		auto renderer = RHI::ImGuiRendererCreate(dev.get());
		PIX_CHECK(renderer && renderer->Init());
		for (int frame = 0; frame < 3; ++frame) {
			renderer->NewFrame();
			ImGui::NewFrame();
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImVec2((float)W, (float)H));
			ImGui::Begin("Test", nullptr, ImGuiWindowFlags_NoTitleBar);
			ImGui::Text("Pixeon");
			ImGui::End();
			ImGui::Render();
			const float black[4] = { 0, 0, 0, 1 };
			dev->RenderTargetClear(dev->BackBufferGet(), black);
			BackBufferBind(dev.get());
			renderer->Render(ImGui::GetDrawData());
			if (frame == 2) {
				Image img = Read(dev.get(), dev->BackBufferGet());
				// ウィンドウ背景が塗られ、文字の明るいピクセルがあること
				int bright = 0;
				for (uint32_t y = 0; y < H; ++y) for (uint32_t x = 0; x < W; ++x) if (img.At(x, y).r > 180) ++bright;
				std::printf("  bright pixels: %d\n", bright);
				PIX_CHECK(img.At(40, 40).r > 0 || img.At(40, 40).g > 0 || img.At(40, 40).b > 0);
				PIX_CHECK(bright > 5);
			}
			dev->Present();
		}
		renderer->Shutdown();
	}
	ImGui::DestroyContext();
	PIX_CHECK(g_ValidationMessages.empty());
}

int main()
{
	return PixTest::AllRun();
}
