#pragma once
#include <string>
#include <memory>
#include <vector>
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>

struct TextureResource {
    std::string name;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    uint32_t width = 0;
    uint32_t height = 0;
    size_t   gpuBytes = 0;
};

struct SubMesh {
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
    bool     skinned = false;
    bool     hasUV = false;
    bool     uvAllZero = false;
};

struct MaterialShared {
    std::string       baseColorTex;
    DirectX::XMFLOAT4 baseColor{ 1,1,1,1 };
    float metallic = 0.0f;
    float roughness = 0.8f;
};

struct Bone {
    std::string name;
    int parentIndex = -1;
    int nodeIndex = -1;         // ★ 追加: ノード階層上の index
    DirectX::XMMATRIX offset;     // InverseBindPose(aiBone::mOffsetMatrix)
    DirectX::XMMATRIX invOffset;  // BindPose(必要なら保持。未使用なら省略可)
};

//struct AnimationChannel {
//    int nodeIndex = -1;
//    std::string nodeName;
//    std::vector<std::pair<float, DirectX::XMFLOAT3>> positionKeys;
//    std::vector<std::pair<float, DirectX::XMFLOAT4>> rotationKeys;
//    std::vector<std::pair<float, DirectX::XMFLOAT3>> scaleKeys;
//};

//struct AnimationClip {
//    std::string name;
//    double duration = 0;
//    double tps = 25.0;
//    std::vector<AnimationChannel> channels;
//    struct NodeInfo {
//        std::string name;
//        int parentIndex = -1;
//        DirectX::XMMATRIX localTransform;
//        std::vector<int> children;
//    };
//    std::vector<NodeInfo> nodeHierarchy;
//};

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
    uint32_t boneIndices[4];
    float boneWeights[4];
};