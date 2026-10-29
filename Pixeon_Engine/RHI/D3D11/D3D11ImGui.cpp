/*
* ファイル名　D3D11ImGui
* 説　　　明　D3D11 バックエンド用の ImGui 描画（imgui_impl_dx11 のラッパー）
*/
#include "../RHI.h"
#include "D3D11Native.h"
#include "../../IMGUI/imgui.h"
#include "../../IMGUI/imgui_impl_dx11.h"

namespace RHI {
	namespace {
		class D3D11ImGuiRenderer final : public ImGuiRenderer {
		public:
			explicit D3D11ImGuiRenderer(Device* device) : m_cls_Device(device) {}

			bool Init() override
			{
				const NativeHandles h = m_cls_Device->NativeHandlesGet();
				return ImGui_ImplDX11_Init((ID3D11Device*)h.p_Device, (ID3D11DeviceContext*)h.p_Context);
			}
			void Shutdown() override { ImGui_ImplDX11_Shutdown(); }
			void NewFrame() override { ImGui_ImplDX11_NewFrame(); }
			void Render(ImDrawData* draw_data) override { ImGui_ImplDX11_RenderDrawData(draw_data); }
			void* TextureIdGet(Texture* texture) override { return D3D11SrvGet(texture); }
			void DeviceObjectsInvalidate() override { ImGui_ImplDX11_InvalidateDeviceObjects(); }
			void DeviceObjectsCreate() override { ImGui_ImplDX11_CreateDeviceObjects(); }

		private:
			Device* m_cls_Device;
		};
	}

	std::unique_ptr<ImGuiRenderer> D3D11ImGuiRendererCreate(Device* device)
	{
		return std::make_unique<D3D11ImGuiRenderer>(device);
	}
}
