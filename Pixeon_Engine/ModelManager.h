#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>


struct NodeInfo {
    std::string name;
    int parentIndex;
    DirectX::XMMATRIX bindLocal;
    std::vector<int> children;
};

struct BoneInfo {
    std::string name;
    int nodeIndex;
    int parentIndex;
    DirectX::XMMATRIX invBind;
};

struct AnimeTransform {
    DirectX::XMFLOAT3 translation;
    DirectX::XMFLOAT4 rotation;
    DirectX::XMFLOAT3 scale;
};

struct AnimationChannel {
    int nodeIndex;
    std::map<float, AnimeTransform> timeline;
};

struct AnimationClip {
    std::string name;
    float durationSeconds;
    float ticksPerSecond;
    std::vector<AnimationChannel> channels;
    std::vector<NodeInfo> nodesCopy;
};

struct ModelVertexNew {
    float position[3];
    float normal[3];
    float uv[2];
    unsigned int boneIndices[4];
    float boneWeights[4];
};

struct SubMeshNew {
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t materialIndex;
    bool skinned;
};

struct MaterialNew {
    DirectX::XMFLOAT4 baseColor;
    std::string baseColorTex;
};

struct ModelSharedResourceNew {
    std::string source;
    std::vector<NodeInfo> nodes;
    std::vector<BoneInfo> bones;
    std::vector<AnimationClip> clips;
    std::vector<SubMeshNew> submeshes;
    std::vector<MaterialNew> materials;

    Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> ib;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

class ModelManagerRebuild {
public:
    static ModelManagerRebuild* Instance();
    std::shared_ptr<ModelSharedResourceNew> Load(const std::string& logicalName);

private:
    void BuildNodeHierarchy(aiNode* node, int parent, std::vector<NodeInfo>& nodes);
    void ProcessMesh(aiMesh* mesh, const aiScene* scene,
        std::vector<ModelVertexNew>& vertices,
        std::vector<uint32_t>& indices,
        std::vector<SubMeshNew>& submeshes,
        std::vector<BoneInfo>& bones,
        const std::vector<NodeInfo>& nodes);
    void ProcessAnimations(const aiScene* scene,
        std::vector<AnimationClip>& clips,
        const std::vector<NodeInfo>& nodes);
    AnimeTransform MakeAnimeTransform(aiVector3D t, aiQuaternion q, aiVector3D s);
    int FindNodeIndex(const std::vector<NodeInfo>& nodes, const std::string& name);
    void FinalizeBoneParents(std::vector<BoneInfo>& bones);

private:
    static ModelManagerRebuild* s_inst;
};