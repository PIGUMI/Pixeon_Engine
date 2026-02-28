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

	void DrawForGBuffer(int Layer);
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

	void SetGlobalOffset(const DirectX::XMFLOAT3& offset) { m_globalOffset = offset; }
	DirectX::XMFLOAT3 GetGlobalOffset() const { return m_globalOffset; }

	void SetGlobalScale(const DirectX::XMFLOAT3& scale) { m_globalScale = scale; }
	DirectX::XMFLOAT3 GetGlobalScale() const { return m_globalScale; }

	void SetGlobalRotation(const DirectX::XMFLOAT3& rotation) { m_globalRotation = rotation; }
	DirectX::XMFLOAT3 GetGlobalRotation() const { return m_globalRotation; }

	std::shared_ptr<ModelSharedResource> GetModel() const { return m_model; }

	// ボーン数を取得
	size_t GetBoneCount() const { return m_model ? m_model->bones.size() : 0; }

	// ボーン名からインデックスを取得
	int GetBoneIndexByName(const std::string& boneName) const;

	// インデックスからボーン名を取得
	std::string GetBoneNameByIndex(int boneIndex) const;

	// ボーンのワールド行列を取得
	DirectX::XMMATRIX GetBoneWorldMatrix(int boneIndex) const;

	// ボーンのワールド位置を取得
	DirectX::XMFLOAT3 GetBoneWorldPosition(int boneIndex) const;

	DirectX::XMFLOAT4 GetBoneWorldRotationQuaternion(int boneIndex) const;

	DirectX::XMFLOAT3 GetBoneWorldRotation(int boneIndex) const;

	DirectX::XMFLOAT3 GetBoneWorldRotationDegrees(int boneIndex) const;

	DirectX::XMFLOAT3 GetBoneLocalPosition(int boneIndex) const;
	DirectX::XMFLOAT3 GetBoneLocalRotation(int boneIndex) const;
	DirectX::XMFLOAT3 GetBoneLocalRotationDegrees(int boneIndex) const;

	// すべてのボーン情報を取得
	const std::vector<Bone>* GetBones() const { return m_model ? &m_model->bones : nullptr; }

	// ボーンの親インデックスを取得
	int GetBoneParentIndex(int boneIndex) const;

	// ボーンの子ボーンリストを取得
	std::vector<int> GetBoneChildren(int boneIndex) const;

private:
	struct CBData {
		DirectX::XMMATRIX World;
		DirectX::XMMATRIX View;
		DirectX::XMMATRIX Proj;
		DirectX::XMFLOAT4 BaseColor;
	};

	struct CameraCBData {
		DirectX::XMFLOAT3 CameraPos;
		float _pad;
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
	Microsoft::WRL::ComPtr<ID3D11Buffer>        m_cameraCb;

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

	DirectX::XMFLOAT3 m_globalOffset{ 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_globalScale{ 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 m_globalRotation{ 0.0f, 0.0f, 0.0f };

	// ボーン表示用の状態管理
	bool m_showBoneHierarchy = false;
	std::unordered_map<int, bool> m_boneTreeOpenState;  // ボーンツリーの開閉状態
	int m_selectedBoneIndex = -1;  // 選択中のボーン
	char m_boneFilterBuffer[128] = "";  // ボーンフィルタ用バッファ

	// ボーン階層表示の再帰関数
	void DrawBoneHierarchyRecursive(int boneIndex, int depth = 0);

	// ボーン情報の詳細表示
	void DrawBoneDetails(int boneIndex);
};