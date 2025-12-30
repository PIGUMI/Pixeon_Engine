#pragma once
#include "Component.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "ShaderManager.h"

#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <functional>

class ModelRenderComponent : public AbstractComponent
{
private:
	enum class TextureIssue : uint8_t {
		None = 0,
		MaterialIndexOutOfRange,
		MaterialNoPath,
		TextureLoadFailed,
		TextureSRVNull,
		NoUVChannel,
		UVAllZero,
		SamplerMissing,
		StillFallbackWhite,
		StillFallbackMagenta
	};

public:
	enum class CullMode : uint8_t {
		Back = 0,
		Front = 1,
		None = 2
	};

	ModelRenderComponent() = default;
	~ModelRenderComponent() = default;

	void Init(AbstractObject* owner) override;
	void Draw(int Layer) override;
	void DrawInspector() override;

	bool SetModel(const std::string& logicalPath);
	const std::string& GetModelPath() const { return m_modelPath; }

	void SetColor(const DirectX::XMFLOAT4& c) { m_color = c; }
	DirectX::XMFLOAT4 GetColor() const { return m_color; }

	void SaveToFile(std::ostream& out) override;
	void LoadFromFile(std::istream& in) override;

	void SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& matrices);
	const std::vector<DirectX::XMFLOAT4X4>& GetBoneMatrices() const { return m_boneMatrices; }
	bool HasBoneMatrices() const { return !m_boneMatrices.empty(); }
	void SetupBoneMatricesForShader(ID3D11DeviceContext* ctx);

	bool SetMaterialTexture(int materialIndex, const std::string& texLogicalPath);

	void SetMeshOffset(size_t meshIndex, const DirectX::XMFLOAT3& offset);
	DirectX::XMFLOAT3 GetMeshOffset(size_t meshIndex) const;

	void SetMeshScale(size_t meshIndex, const DirectX::XMFLOAT3& scale);
	DirectX::XMFLOAT3 GetMeshScale(size_t meshIndex) const;

	void SetMeshCullMode(size_t meshIndex, CullMode mode);
	CullMode GetMeshCullMode(size_t meshIndex) const;

private:
	struct CBData {
		DirectX::XMMATRIX World;
		DirectX::XMMATRIX View;
		DirectX::XMMATRIX Proj;
		DirectX::XMFLOAT4 BaseColor;
	};
	struct MaterialRuntime {
		std::string                          texName;
		std::shared_ptr<TextureResource>     tex;
		DirectX::XMFLOAT4                    color;
		DirectX::XMFLOAT3                    meshOffset;
		DirectX::XMFLOAT3                    meshScale;
		CullMode                             cullMode;
	};

	bool EnsureShaders(bool forceRecreateLayout = false);
	bool EnsureInputLayout(const void* vsBytecode, size_t size);
	bool EnsureConstantBuffer();
	void RefreshMaterialCache();

	void ShowModelSelectPopup();
	void ShowTextureSelectPopup(int materialIndex);

	void RecreateInputLayout();
	DirectX::XMMATRIX BuildWorldMatrix() const;
	DirectX::XMMATRIX BuildMeshWorldMatrix(const DirectX::XMFLOAT3& offset, const DirectX::XMFLOAT3& scale) const;

	bool EnsureWhiteTexture();
	bool EnsureDebugFallbackTextures();
	bool EnsureRasterizerStates();

	void DiagnoseAndReportTextureIssue(size_t submeshIdx,
		const SubMesh& sm,
		const MaterialRuntime* mat,
		ID3D11ShaderResourceView* chosenSRV,
		bool usedMagentaFallback,
		bool usedWhiteFallback);

	void EnsureDefaultBoneMatrices();

private:
	std::string m_modelPath;
	std::shared_ptr<ModelSharedResource> m_model;
	std::vector<MaterialRuntime> m_materials;

	DirectX::XMFLOAT4 m_color{ 1,1,1,1 };

	Microsoft::WRL::ComPtr<ID3D11Buffer>        m_cb;

	Microsoft::WRL::ComPtr<ID3D11VertexShader>  m_vs;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_ps;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_layout;

	std::string m_vsName = "VS_ModelStatic";
	std::string m_psName = "PS_ModelStatic";

	static Microsoft::WRL::ComPtr<ID3D11SamplerState>        s_linearSmp;
	static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  s_whiteTexSRV;
	static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  s_magentaTexSRV;

	// ラスタライザーステート
	static Microsoft::WRL::ComPtr<ID3D11RasterizerState>     s_rasterizerCullBack;
	static Microsoft::WRL::ComPtr<ID3D11RasterizerState>     s_rasterizerCullFront;
	static Microsoft::WRL::ComPtr<ID3D11RasterizerState>     s_rasterizerCullNone;

	std::vector<uint8_t> m_texIssueReported;

	bool m_ready = false;
	bool m_openTexPopup = false;
	int  m_texPopupMatIndex = -1;

	std::vector<DirectX::XMFLOAT4X4> m_boneMatrices;
	bool m_useBoneMatrices = false;
};