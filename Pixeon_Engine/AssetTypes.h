#pragma once
#include <string>
#include <memory>
#include <vector>
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>

// テクスチャ共有リソース
struct TextureResource {
	std::string name;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	uint32_t width = 0;
	uint32_t height = 0;
	size_t   gpuBytes = 0;
};

struct MaterialShared {
	DirectX::XMFLOAT4 baseColor{ 1,1,1,1 };
	std::string baseColorTex;
	bool isEmbedded = false;  // 追加: 埋め込みテクスチャかどうか
};

// SubMesh構造体に頂点カラーフラグを追加
struct SubMesh {
	uint32_t indexOffset = 0;
	uint32_t indexCount = 0;
	size_t materialIndex = 0;
	bool skinned = false;
	bool hasUV = true;
	bool uvAllZero = false;
	bool hasVertexColors = false;  // 追加
};

// ------------------------------------------------------------
// Bone.offset : InverseBindPose (aiBone::mOffsetMatrix そのまま)
// Bone.invOffset : BindPose = inverse(InverseBindPose)
// nodeIndex : アニメ用ノード階層(clip.nodeHierarchy)上の index
// ------------------------------------------------------------
struct Bone {
	std::string name;
	int parentIndex = -1;
	int nodeIndex = -1;
	DirectX::XMMATRIX offset;     // InverseBindPose
	DirectX::XMMATRIX invOffset;  // BindPose = inverse(offset)
};

struct AnimationChannel {
	int nodeIndex = -1;
	std::string nodeName;
	std::vector<std::pair<float, DirectX::XMFLOAT3>> positionKeys;
	std::vector<std::pair<float, DirectX::XMFLOAT4>> rotationKeys;
	std::vector<std::pair<float, DirectX::XMFLOAT3>> scaleKeys;
};

struct AnimationClip {
	std::string name;
	double duration = 0;
	double tps = 25.0;
	std::vector<AnimationChannel> channels;
	struct NodeInfo {
		std::string name;
		int parentIndex = -1;
		DirectX::XMMATRIX localTransform;
		std::vector<int> children;
	};
	std::vector<NodeInfo> nodeHierarchy;
};

struct ModelSharedResource {
	std::string source;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ib;
	uint32_t vertexCount = 0;
	uint32_t indexCount = 0;
	std::vector<SubMesh> submeshes;
	std::vector<MaterialShared> materials;
	std::vector<Bone> bones;
	std::vector<AnimationClip> clips;
	bool hasSkin = false;
	size_t gpuBytes = 0;
};

struct SoundResource {
	std::string name;
	std::vector<uint8_t> pcmData;
	int channels = 0;
	int sampleRate = 0;
	bool streaming = false;
};

struct ModelVertex {
	float position[3];
	float normal[3];
	float tangent[4];
	float uv[2];
	float color[4];  // 追加:  頂点カラー
	uint32_t boneIndices[4];
	float boneWeights[4];
};