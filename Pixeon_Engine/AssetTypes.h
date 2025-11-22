// アセットの種類を定義するヘッダーファイル
// 2025/09/29 By Akino
#ifndef ASSETTYPES_H
#define ASSETTYPES_H

#include <string>
#include <memory>
#include <vector>
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>

// テクスチャリソース
struct TextureResource {
	std::string name; // テクスチャ名
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv; // シェーダーリソースビュー
	uint32_t width		= 0; // テクスチャの幅
	uint32_t height		= 0; // テクスチャの高さ
	size_t	gpuBytes	= 0; // GPUメモリ使用量
};

// SubMesh情報
struct SubMesh {
    uint32_t indexOffset = 0;   // インデックスオフセット
    uint32_t indexCount = 0;   // インデックス数
    uint32_t materialIndex = 0;   // マテリアルインデックス
    bool     skinned = false; // スキン有無
    bool     hasUV = false; // UVチャネルを持つ
    bool     uvAllZero = false; // UVが全て(0,0)
};

// マテリアル共通データ
struct MaterialShared {
	std::string baseColorTex;				// テクスチャ名
	DirectX::XMFLOAT4 baseColor{ 1,1,1,1 }; // ベースカラー
	float metallic	= 0.0f;					// メタリック
	float roughness = 0.8f;					// ラフネス
};

// ボーン情報
struct Bone {
    std::string name;
    int parentIndex = -1;
    DirectX::XMMATRIX offset;
    DirectX::XMMATRIX invOffset;
};

// アニメーションチャンネル
struct AnimationChannel {
    int nodeIndex = -1;
    std::string nodeName;
    // Position keys
    std::vector<std::pair<float, DirectX::XMFLOAT3>> positionKeys;
    // Rotation keys (quaternion)
    std::vector<std::pair<float, DirectX::XMFLOAT4>> rotationKeys;
    // Scale keys
    std::vector<std::pair<float, DirectX::XMFLOAT3>> scaleKeys;
};

// アニメーションクリップ
struct AnimationClip {
    std::string name;
    double duration = 0;
    double tps = 25.0;
    std::vector<AnimationChannel> channels;
    // ノード階層情報
    struct NodeInfo {
        std::string name;
        int parentIndex = -1;
        DirectX::XMMATRIX localTransform;
        std::vector<int> children;
    };
    std::vector<NodeInfo> nodeHierarchy;
};

// モデル共通データ
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

// サウンドリソース
struct SoundResource {
    std::string name;
    std::vector<uint8_t> pcmData;
    int channels = 0;
    int sampleRate = 0;
    bool streaming = false;
};

// モデル頂点フォーマット
struct ModelVertex {
    float position[3];
    float normal[3];
    float tangent[4];
    float uv[2];
    uint32_t boneIndices[4];
    float boneWeights[4];
};

#endif // ASSETTYPES_H