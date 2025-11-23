#pragma once
#include "AssetTypes.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <map>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class ModelManager {
public:
    static ModelManager* Instance();
    static void DeleteInstance();
    std::shared_ptr<ModelSharedResource> LoadOrGet(const std::string& logicalName);
    void UnInit();
    void GarbageCollect();
    void DrawDebugGUI();
private:
    ModelManager() = default;
    std::shared_ptr<ModelSharedResource> LoadInternal(const std::string& logicalName);

    void ProcessNode(aiNode* node, const aiScene* scene,
        std::vector<ModelVertex>& vertices,
        std::vector<uint32_t>& indices,
        std::shared_ptr<ModelSharedResource> shared);

    void ProcessMesh(aiMesh* mesh, const aiScene* scene,
        std::vector<ModelVertex>& vertices,
        std::vector<uint32_t>& indices,
        std::shared_ptr<ModelSharedResource> shared);

    void ProcessMaterials(const aiScene* scene,
        std::shared_ptr<ModelSharedResource> shared);

    // 修正: 重複追加禁止 / 親インデックス設定のみ
    void ProcessBonesFinalizeHierarchy(const aiScene* scene,
        std::shared_ptr<ModelSharedResource> shared);

    void ProcessAnimations(const aiScene* scene,
        std::shared_ptr<ModelSharedResource> shared);

    void BuildNodeHierarchy(aiNode* node,
        AnimationClip& clip,
        std::map<std::string, int>& nodeNameToIndex,
        int parentIndex);

    std::string ResolveTexturePath(const std::string& modelLogical,
        const std::string& rawPath);

    struct Entry {
        std::weak_ptr<ModelSharedResource> weak;
        uint64_t lastUse = 0;
        size_t gpuBytes = 0;
    };
    std::unordered_map<std::string, Entry> m_cache;
    uint64_t m_frame = 0;
    std::mutex m_mtx;
    static ModelManager* s_instance;
};