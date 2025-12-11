#ifndef IMAGE_RENDER_H
#define IMAGE_RENDER_H
// 画像レンダリングコンポーネント
// 2025/10/13

#include "Component.h"
#include "ResourceService.h"
#include "TextureManager.h"
#include "ShaderManager.h"
#include "Scene.h"
#include "CameraComponent.h"
#include "GUI.h"
#include "AssetManager.h"
#include "System.h"

#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>

class ImageRender : public Component {
public:
	enum class PlacementMode : int {
		Screen2D = 0,
		Billboard = 1,
		World3D = 2,
		UI = 3
	};
public:
	ImageRender();
	~ImageRender();

	void Init(Object* owner) override;
	void InGameUpdate() override;
	void EditUpdate() override;
	void Update();
	void Draw() override;
	void DrawInspector() override;
	void UInit() override;

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;
	void SetCamera(CameraComponent* ptr);

	void SetTextureName(const std::string& name);
	const std::string& GetTextureName() const { return m_textureName; }

	DirectX::XMFLOAT2 GetSize2D() const { return m_size2D; }
	void SetSize2D(const DirectX::XMFLOAT2& size) { m_size2D = size; }
	DirectX::XMFLOAT2 GetSizeWorld() const { return m_sizeWorld; }
	void SetSizeWorld(const DirectX::XMFLOAT2& size) { m_sizeWorld = size; }
	PlacementMode GetPlacementMode() const { return m_mode; }
	void SetPlacementMode(PlacementMode mode) { m_mode = mode; }
	DirectX::XMFLOAT4 GetUVRect() const { return m_uvRect; }
	void SetUVRect(const DirectX::XMFLOAT4& rect) { m_uvRect = rect; }
	DirectX::XMFLOAT4 GetColor() const { return m_color; }
	void SetColor(const DirectX::XMFLOAT4& color) { m_color = color; }
	DirectX::XMFLOAT2 GetOffset2D() const { return m_offset2D; }
	void SetOffset2D(const DirectX::XMFLOAT2& offset) { m_offset2D = offset; }
	DirectX::XMFLOAT3 GetOffset3D() const { return m_offset3D; }
	void SetOffset3D(const DirectX::XMFLOAT3& offset) { m_offset3D = offset; }

private:
	struct Vertex {
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 uv;
	};
	struct CBVS {
		DirectX::XMMATRIX View;
		DirectX::XMMATRIX Proj;
		DirectX::XMFLOAT4 Color; // 色
		int mode2D;
		float pad[3];            // 16Bアラインメント
	};

	// 内部処理
	bool EnsureShaders(bool forceRecreateLayout = false);
	void RecreateInputLayout();
	bool EnsureInputLayout(const void* vsBytecode, size_t size);
	bool EnsureConstantBuffer();
	bool EnsureBuffers();
	bool EnsureBlendState();
	bool EnsureDepthStencilState();

	void UpdateVertices2D(Vertex outV[4], float& outZClip);
	void UpdateVerticesBillboard(Vertex outV[4]);
	void UpdateVerticesWorld3D(Vertex outV[4]);
	void UpdateVerticesUI(Vertex outV[4]);

	void UpdateVB(const Vertex v[4]);

	static bool EnsureFallbackTextures();
	static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> s_whiteTexSRV;
	static Microsoft::WRL::ComPtr<ID3D11SamplerState> s_linearSmp;
	static Microsoft::WRL::ComPtr<ID3D11BlendState> s_alphaBlendState;
	static Microsoft::WRL::ComPtr<ID3D11DepthStencilState> s_depthStencilState;
private:
	// リソース
	std::string m_textureName;
	std::shared_ptr<TextureResource> m_texture;

	// 表示設定
	PlacementMode m_mode = PlacementMode::Screen2D;
	CameraComponent* cam = nullptr;
	// 2D: ピクセル単位, 3D/Billboard: ワールド単位
	DirectX::XMFLOAT2 m_size2D = { 128.0f, 128.0f };
	DirectX::XMFLOAT2 m_sizeWorld = { 1.0f, 1.0f };

	// オフセット
	DirectX::XMFLOAT2 m_offset2D = { 0.0f, 0.0f };       // 2D用 (px)
	DirectX::XMFLOAT3 m_offset3D = { 0.0f, 0.0f, 0.0f }; // 3D/Billboard用 (world)

	// UV矩形 [0..1]
	DirectX::XMFLOAT4 m_uvRect = { 0.0f, 0.0f, 1.0f, 1.0f }; // u0,v0,u1,v1

	DirectX::XMFLOAT4 m_color = { 1,1,1,1 };

	// シェーダ名
	std::string m_vsName = "VS_ImgQuad";
	std::string m_psName = "PS_ImgQuad";

	// D3D
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_ps;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       m_cbVS;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       m_vb;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       m_ib;

	bool m_ready = false;
};

#endif // IMAGE_RENDER_H