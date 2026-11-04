/*
* ファイル名　RHIImGui
* 説　　　明　RHI の API だけで書いた ImGui 描画（D3D12 / Vulkan 共通）
*             D3D11 は公式の imgui_impl_dx11 を使うが、D3D12 / Vulkan は RHI 側でディスクリプタや
*             状態遷移を管理しているため、RHI 上に描画処理を載せたほうが整合が取りやすい
*
*   ImTextureID は RHI::Texture* をそのまま使う
*   フォントアトラスは ImGui 1.92 のテクスチャ要求 (ImGuiBackendFlags_RendererHasTextures) に従って作成・更新する
*/
#include "RHI.h"
#include "RHIImGuiShaders.h"
#include "../IMGUI/imgui.h"
#if defined(PIXEON_RHI_D3D12)
#include "DXGI/DXGICommon.h"
#endif
#include <cstring>

#if defined(PIXEON_RHI_D3D12) || (defined(PIXEON_RHI_VULKAN) && !defined(PIXEON_RHI_NO_IMGUI))

namespace RHI {
	namespace {
		struct ImGuiConstants {
			float f_ScaleTranslate[4];
		};

		class RhiImGuiRenderer final : public ImGuiRenderer {
		public:
			explicit RhiImGuiRenderer(Device* device) : m_cls_Device(device) {}
			~RhiImGuiRenderer() override { if (m_b_Initialized) Shutdown(); }

			bool Init() override;
			void Shutdown() override;
			void NewFrame() override;
			void Render(ImDrawData* draw_data) override;
			void* TextureIdGet(Texture* texture) override { return texture; }
			void DeviceObjectsInvalidate() override;
			void DeviceObjectsCreate() override { ObjectsCreate(); }

		private:
			bool ObjectsCreate();
			void TextureProcess(ImTextureData* tex);
			void TextureDestroy(ImTextureData* tex);
			void StateSetup(ImDrawData* draw_data);

			Device* m_cls_Device;
			bool m_b_Initialized = false;
			ShaderRef m_cls_Vs;
			ShaderRef m_cls_Ps;
			InputLayoutRef m_cls_Layout;
			BufferRef m_cls_Constants;
			BufferRef m_cls_Vb;
			BufferRef m_cls_Ib;
			uint32_t m_i_VbCount = 0;
			uint32_t m_i_IbCount = 0;
			BlendStateRef m_cls_Blend;
			DepthStencilStateRef m_cls_Depth;
			RasterizerStateRef m_cls_Raster;
			SamplerRef m_cls_Sampler;
		};

		bool RhiImGuiRenderer::Init()
		{
			ImGuiIO& io = ImGui::GetIO();
			io.BackendRendererName = (m_cls_Device->BackendGet() == Backend::Vulkan) ? "pixeon_rhi_vulkan" : "pixeon_rhi_d3d12";
			io.BackendRendererUserData = this;
			io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
			io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
			ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
			pio.Renderer_TextureMaxWidth = pio.Renderer_TextureMaxHeight = 16384;
			m_b_Initialized = true;
			return ObjectsCreate();
		}

		void RhiImGuiRenderer::Shutdown()
		{
			if (!m_b_Initialized) return;
			DeviceObjectsInvalidate();
			ImGuiIO& io = ImGui::GetIO();
			io.BackendRendererName = nullptr;
			io.BackendRendererUserData = nullptr;
			io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
			m_b_Initialized = false;
		}

		void RhiImGuiRenderer::NewFrame()
		{
			if (!m_cls_Vs) ObjectsCreate();
		}

		/*
		* 関数名　ObjectsCreate
		* 引　数　なし
		* 戻り値　成功なら true
		* 説　明　シェーダー・頂点レイアウト・ステートを作る
		*/
		bool RhiImGuiRenderer::ObjectsCreate()
		{
			Device* dev = m_cls_Device;
			if (dev->BackendGet() == Backend::Vulkan) {
				m_cls_Vs = dev->ShaderCreate(ShaderStage::Vertex, IMGUI_VS_SPIRV, sizeof(IMGUI_VS_SPIRV), "ImGui_VS");
				m_cls_Ps = dev->ShaderCreate(ShaderStage::Pixel, IMGUI_PS_SPIRV, sizeof(IMGUI_PS_SPIRV), "ImGui_PS");
			}
#if defined(PIXEON_RHI_D3D12)
			else {
				std::vector<uint8_t> vs, ps;
				std::string error;
				const size_t len = std::strlen(IMGUI_SHADER_HLSL);
				if (!DxbcCompile(IMGUI_SHADER_HLSL, len, "VS_Main", "vs_5_0", vs, &error) ||
					!DxbcCompile(IMGUI_SHADER_HLSL, len, "PS_Main", "ps_5_0", ps, &error)) {
					LogWrite("ImGui: シェーダーのコンパイルに失敗 %s", error.c_str());
					return false;
				}
				m_cls_Vs = dev->ShaderCreate(ShaderStage::Vertex, vs.data(), vs.size(), "ImGui_VS");
				m_cls_Ps = dev->ShaderCreate(ShaderStage::Pixel, ps.data(), ps.size(), "ImGui_PS");
			}
#endif
			if (!m_cls_Vs || !m_cls_Ps) return false;

			const InputElement elements[] = {
				{ "POSITION", 0, Format::R32G32_Float,   0, (uint32_t)offsetof(ImDrawVert, pos) },
				{ "TEXCOORD", 0, Format::R32G32_Float,   0, (uint32_t)offsetof(ImDrawVert, uv) },
				{ "COLOR",    0, Format::R8G8B8A8_UNorm, 0, (uint32_t)offsetof(ImDrawVert, col) },
			};
			m_cls_Layout = dev->InputLayoutCreate(elements, 3, m_cls_Vs.get());

			BufferDesc cd;
			cd.i_Size = sizeof(ImGuiConstants);
			cd.i_BindFlags = Bind_ConstantBuffer;
			cd.e_Usage = Usage::Dynamic;
			m_cls_Constants = dev->BufferCreate(cd, nullptr);

			BlendDesc bd;
			auto& rt = bd.renderTarget[0];
			rt.b_BlendEnable = true;
			rt.e_SrcBlend = Blend::SrcAlpha;
			rt.e_DestBlend = Blend::InvSrcAlpha;
			rt.e_SrcBlendAlpha = Blend::One;
			rt.e_DestBlendAlpha = Blend::InvSrcAlpha;
			m_cls_Blend = dev->BlendStateCreate(bd);

			DepthStencilDesc dd;
			dd.b_DepthEnable = false;
			dd.b_DepthWrite = false;
			dd.e_DepthFunc = CompareFunc::Always;
			m_cls_Depth = dev->DepthStencilStateCreate(dd);

			RasterizerDesc rd;
			rd.e_Cull = CullMode::None;
			rd.b_Scissor = true;
			m_cls_Raster = dev->RasterizerStateCreate(rd);

			SamplerDesc sd;
			sd.e_Filter = Filter::Linear;
			sd.e_AddressU = sd.e_AddressV = sd.e_AddressW = AddressMode::Clamp;
			m_cls_Sampler = dev->SamplerCreate(sd);
			return m_cls_Layout && m_cls_Constants && m_cls_Blend && m_cls_Depth && m_cls_Raster && m_cls_Sampler;
		}

		void RhiImGuiRenderer::DeviceObjectsInvalidate()
		{
			if (ImGui::GetCurrentContext()) {
				for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
					if (tex->RefCount == 1) TextureDestroy(tex);
			}
			m_cls_Vs.reset();
			m_cls_Ps.reset();
			m_cls_Layout.reset();
			m_cls_Constants.reset();
			m_cls_Vb.reset();
			m_cls_Ib.reset();
			m_i_VbCount = m_i_IbCount = 0;
			m_cls_Blend.reset();
			m_cls_Depth.reset();
			m_cls_Raster.reset();
			m_cls_Sampler.reset();
		}

		void RhiImGuiRenderer::TextureDestroy(ImTextureData* tex)
		{
			auto* holder = (TextureRef*)tex->BackendUserData;
			if (!holder) return;
			delete holder;   // GPU が使い終わるまでの遅延解放はデバイス側で行う
			tex->SetTexID(ImTextureID_Invalid);
			tex->SetStatus(ImTextureStatus_Destroyed);
			tex->BackendUserData = nullptr;
		}

		/*
		* 関数名　TextureProcess
		* 引　数　tex：ImGui のテクスチャ要求
		* 戻り値　なし
		* 説　明　作成・更新・破棄の要求に応える（更新はテクスチャ全体を送り直す）
		*/
		void RhiImGuiRenderer::TextureProcess(ImTextureData* tex)
		{
			if (tex->Status == ImTextureStatus_WantCreate) {
				TextureDesc td;
				td.i_Width = (uint32_t)tex->Width;
				td.i_Height = (uint32_t)tex->Height;
				td.e_Format = Format::R8G8B8A8_UNorm;
				td.i_BindFlags = Bind_ShaderResource;
				td.c_DebugName = "ImGuiTexture";
				std::vector<uint32_t> converted;
				const void* pixels = tex->GetPixels();
				if (tex->Format == ImTextureFormat_Alpha8) {
					converted.resize((size_t)tex->Width * tex->Height);
					const uint8_t* a = (const uint8_t*)pixels;
					for (size_t i = 0; i < converted.size(); ++i) converted[i] = 0x00FFFFFFu | ((uint32_t)a[i] << 24);
					pixels = converted.data();
				}
				SubresourceData init{ pixels, td.i_Width * 4, 0 };
				TextureRef texture = m_cls_Device->TextureCreate(td, &init);
				if (!texture) return;
				tex->BackendUserData = new TextureRef(texture);
				tex->SetTexID((ImTextureID)(uintptr_t)texture.get());
				tex->SetStatus(ImTextureStatus_OK);
			}
			else if (tex->Status == ImTextureStatus_WantUpdates) {
				auto* holder = (TextureRef*)tex->BackendUserData;
				if (holder && *holder) {
					if (tex->Format == ImTextureFormat_Alpha8) {
						std::vector<uint32_t> converted((size_t)tex->Width * tex->Height);
						const uint8_t* a = (const uint8_t*)tex->GetPixels();
						for (size_t i = 0; i < converted.size(); ++i) converted[i] = 0x00FFFFFFu | ((uint32_t)a[i] << 24);
						m_cls_Device->TextureUpdate(holder->get(), 0, converted.data(), (uint32_t)tex->Width * 4);
					}
					else {
						m_cls_Device->TextureUpdate(holder->get(), 0, tex->GetPixels(), (uint32_t)tex->GetPitch());
					}
				}
				tex->SetStatus(ImTextureStatus_OK);
			}
			if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0) TextureDestroy(tex);
		}

		void RhiImGuiRenderer::StateSetup(ImDrawData* draw_data)
		{
			Device* dev = m_cls_Device;
			Viewport vp;
			vp.f_Width = draw_data->DisplaySize.x * draw_data->FramebufferScale.x;
			vp.f_Height = draw_data->DisplaySize.y * draw_data->FramebufferScale.y;
			dev->ViewportSet(vp);

			// 画面座標 → クリップ座標: x' = x * 2/W - (2L/W + 1)、y' = -(y * 2/H) + (2T/H + 1)
			const float L = draw_data->DisplayPos.x, T = draw_data->DisplayPos.y;
			const float W = draw_data->DisplaySize.x, H = draw_data->DisplaySize.y;
			ImGuiConstants c;
			c.f_ScaleTranslate[0] = 2.0f / W;
			c.f_ScaleTranslate[1] = -2.0f / H;
			c.f_ScaleTranslate[2] = -1.0f - L * 2.0f / W;
			c.f_ScaleTranslate[3] = 1.0f + T * 2.0f / H;
			dev->BufferUpdate(m_cls_Constants.get(), &c, sizeof(c));

			dev->ShaderSet(ShaderStage::Vertex, m_cls_Vs.get());
			dev->ShaderSet(ShaderStage::Pixel, m_cls_Ps.get());
			dev->InputLayoutSet(m_cls_Layout.get());
			dev->TopologySet(Topology::TriangleList);
			dev->VertexBufferSet(0, m_cls_Vb.get(), sizeof(ImDrawVert), 0);
			dev->IndexBufferSet(m_cls_Ib.get(), sizeof(ImDrawIdx) == 2 ? IndexFormat::UInt16 : IndexFormat::UInt32, 0);
			dev->ConstantBufferSet(ShaderStage::Vertex, 0, m_cls_Constants.get());
			dev->SamplerSet(ShaderStage::Pixel, 0, m_cls_Sampler.get());
			const float factor[4] = { 0, 0, 0, 0 };
			dev->BlendStateSet(m_cls_Blend.get(), factor, 0xFFFFFFFFu);
			dev->DepthStencilStateSet(m_cls_Depth.get(), 0);
			dev->RasterizerStateSet(m_cls_Raster.get());
		}

		/*
		* 関数名　Render
		* 引　数　draw_data：ImGui の描画データ
		* 戻り値　なし
		* 説　明　現在のレンダーターゲットへ描き、変更したステートを元に戻す
		*/
		void RhiImGuiRenderer::Render(ImDrawData* draw_data)
		{
			if (!draw_data || draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f) return;
			if (!m_cls_Vs && !ObjectsCreate()) return;
			Device* dev = m_cls_Device;

			if (draw_data->Textures)
				for (ImTextureData* tex : *draw_data->Textures)
					if (tex->Status != ImTextureStatus_OK) TextureProcess(tex);
			if (draw_data->TotalVtxCount <= 0) return;

			// 頂点・インデックスバッファ（足りなければ作り直す）
			if (!m_cls_Vb || m_i_VbCount < (uint32_t)draw_data->TotalVtxCount) {
				m_i_VbCount = (uint32_t)draw_data->TotalVtxCount + 5000;
				BufferDesc d;
				d.i_Size = m_i_VbCount * sizeof(ImDrawVert);
				d.i_BindFlags = Bind_VertexBuffer;
				d.e_Usage = Usage::Dynamic;
				m_cls_Vb = dev->BufferCreate(d, nullptr);
			}
			if (!m_cls_Ib || m_i_IbCount < (uint32_t)draw_data->TotalIdxCount) {
				m_i_IbCount = (uint32_t)draw_data->TotalIdxCount + 10000;
				BufferDesc d;
				d.i_Size = m_i_IbCount * sizeof(ImDrawIdx);
				d.i_BindFlags = Bind_IndexBuffer;
				d.e_Usage = Usage::Dynamic;
				m_cls_Ib = dev->BufferCreate(d, nullptr);
			}
			if (!m_cls_Vb || !m_cls_Ib) return;
			ImDrawVert* vtx = (ImDrawVert*)dev->BufferMap(m_cls_Vb.get());
			ImDrawIdx* idx = (ImDrawIdx*)dev->BufferMap(m_cls_Ib.get());
			if (!vtx || !idx) return;
			for (const ImDrawList* list : draw_data->CmdLists) {
				std::memcpy(vtx, list->VtxBuffer.Data, list->VtxBuffer.Size * sizeof(ImDrawVert));
				std::memcpy(idx, list->IdxBuffer.Data, list->IdxBuffer.Size * sizeof(ImDrawIdx));
				vtx += list->VtxBuffer.Size;
				idx += list->IdxBuffer.Size;
			}
			dev->BufferUnmap(m_cls_Vb.get());
			dev->BufferUnmap(m_cls_Ib.get());

			// 元のステートを退避
			BlendState* prev_blend = dev->BlendStateGet();
			DepthStencilState* prev_depth = dev->DepthStencilStateGet();
			RasterizerState* prev_raster = dev->RasterizerStateGet();
			const Viewport prev_vp = dev->ViewportGet();

			StateSetup(draw_data);
			const ImVec2 clip_off = draw_data->DisplayPos;
			const ImVec2 clip_scale = draw_data->FramebufferScale;
			int global_vtx = 0, global_idx = 0;
			for (const ImDrawList* list : draw_data->CmdLists) {
				for (int i = 0; i < list->CmdBuffer.Size; ++i) {
					const ImDrawCmd* cmd = &list->CmdBuffer[i];
					if (cmd->UserCallback) {
						if (cmd->UserCallback == ImDrawCallback_ResetRenderState) StateSetup(draw_data);
						else cmd->UserCallback(list, cmd);
						continue;
					}
					const float x0 = (cmd->ClipRect.x - clip_off.x) * clip_scale.x;
					const float y0 = (cmd->ClipRect.y - clip_off.y) * clip_scale.y;
					const float x1 = (cmd->ClipRect.z - clip_off.x) * clip_scale.x;
					const float y1 = (cmd->ClipRect.w - clip_off.y) * clip_scale.y;
					if (x1 <= x0 || y1 <= y0) continue;
					Rect rc;
					rc.i_Left = (int32_t)(x0 > 0 ? x0 : 0);
					rc.i_Top = (int32_t)(y0 > 0 ? y0 : 0);
					rc.i_Right = (int32_t)x1;
					rc.i_Bottom = (int32_t)y1;
					dev->ScissorSet(rc);
					dev->TextureSet(ShaderStage::Pixel, 0, (Texture*)(uintptr_t)cmd->GetTexID());
					dev->IndexedDraw(cmd->ElemCount, cmd->IdxOffset + global_idx, (int32_t)(cmd->VtxOffset + global_vtx));
				}
				global_idx += list->IdxBuffer.Size;
				global_vtx += list->VtxBuffer.Size;
			}

			dev->TextureSet(ShaderStage::Pixel, 0, nullptr);
			dev->BlendStateSet(prev_blend, nullptr, 0xFFFFFFFFu);
			dev->DepthStencilStateSet(prev_depth, 0);
			dev->RasterizerStateSet(prev_raster);
			dev->ViewportSet(prev_vp);
		}
	}

#if defined(PIXEON_RHI_D3D12)
	std::unique_ptr<ImGuiRenderer> D3D12ImGuiRendererCreate(Device* device)
	{
		return std::make_unique<RhiImGuiRenderer>(device);
	}
#endif
#if defined(PIXEON_RHI_VULKAN) && !defined(PIXEON_RHI_NO_IMGUI)
	std::unique_ptr<ImGuiRenderer> VulkanImGuiRendererCreate(Device* device)
	{
		return std::make_unique<RhiImGuiRenderer>(device);
	}
#endif
}

#endif
