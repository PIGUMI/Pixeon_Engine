#define NOMINMAX
#include "ModelManager.h"
#include "AssetManager.h"
#include "System.h"
#include "ErrorLog.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Windows.h>
#include "IMGUI/imgui.h"
#include <filesystem>
#include <unordered_set>

#if _MSC_VER >= 1930
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "assimp-vc143-mt.lib")
#endif
#elif _MSC_VER >= 1920
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc142-mtd.lib")
#else
#pragma comment(lib, "assimp-vc142-mt.lib")
#endif
#elif _MSC_VER >= 1910
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc141-mtd.lib")
#else
#pragma comment(lib, "assimp-vc141-mt.lib")
#endif
#endif

static std::string MM_NormalizePath(std::string s) {
    for (auto& c : s) if (c == '\\') c = '/';
    while (s.size() && (s[0] == '/' || (s.size() >= 2 && s[0] == '.' && s[1] == '/'))) {
        if (s[0] == '/') s.erase(0, 1);
        else if (s.rfind("./", 0) == 0) s.erase(0, 2);
        else break;
    }
    return s;
}

ModelManager* ModelManager::s_instance = nullptr;

ModelManager* ModelManager::Instance() {
    if (!s_instance) {
        s_instance = new ModelManager();
    }
    return s_instance;
}

void ModelManager::DeleteInstance() {
    if (s_instance) {
		s_instance->UnInit();
        delete s_instance;
        s_instance = nullptr;
    }
}

void ModelManager::UnInit() {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_cache.clear();
    m_frame = 0;
}

std::shared_ptr<ModelSharedResource> ModelManager::LoadOrGet(const std::string& logicalName) {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_frame++;

    auto it = m_cache.find(logicalName);
    if (it != m_cache.end()) {
        if (auto sp = it->second.weak.lock()) {
            it->second.lastUse = m_frame;
            return sp;
        }
    }

    auto res = LoadInternal(logicalName);
    if (res) {
        Entry e;
        e.weak = res;
        e.lastUse = m_frame;
        e.gpuBytes = res->gpuBytes;
        m_cache[logicalName] = e;
    }
    return res;
}

std::shared_ptr<ModelSharedResource> ModelManager::LoadInternal(const std::string& logicalName) {
    std::vector<uint8_t> data;
    if (!AssetManager::Instance()->LoadAsset(logicalName, data) || data.empty()) {
		ErrorLogger::Instance().LogError("ModelManager", "Failed to load model asset: " + logicalName);
        return nullptr;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFileFromMemory(
        data.data(), data.size(),
        aiProcess_Triangulate |
        aiProcess_CalcTangentSpace |
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_LimitBoneWeights |
        aiProcess_ImproveCacheLocality |
        aiProcess_SortByPType |
        aiProcess_FlipUVs);

    if (!scene || !scene->mRootNode) {
        ErrorLogger::Instance().LogError("ModelManager", "Assimp parse failed: " + logicalName + 
			(importer.GetErrorString()[0] ? (" (" + std::string(importer.GetErrorString()) + ")") : ""));
        return nullptr;
    }

    auto shared = std::make_shared<ModelSharedResource>();
    shared->source = logicalName;

    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;

    ProcessNode(scene->mRootNode, scene, vertices, indices, shared);

    if (!CreateGPUBuffers(vertices, indices, shared)) {
		ErrorLogger::Instance().LogError("ModelManager", "GPU buffer creation failed: " + logicalName);
        return nullptr;
    }

    ProcessMaterials(scene, shared);

    ProcessBones(scene, shared);

    ProcessAnimations(scene, shared);

    shared->gpuBytes = vertices.size() * sizeof(ModelVertex) + indices.size() * sizeof(uint32_t);

	//ErrorLogger::Instance().LogError("ModelManager", "Load OK: " + logicalName, false, 5);
    return shared;
}

std::string ModelManager::ResolveTexturePath(const std::string& modelLogical, const std::string& rawPath){
    if (rawPath.empty()) return {};
    if (rawPath[0] == '*') { 
		ErrorLogger::Instance().LogError("ModelManager", "Embedded texture unsupported: " + rawPath);
        return {};
    }
    std::string norm = MM_NormalizePath(rawPath);

   
    {
        std::filesystem::path p(norm);
        if (p.is_absolute()) norm = p.filename().generic_string();
    }


    std::string modelDir;
    if (auto pos = modelLogical.find_last_of("/\\"); pos != std::string::npos) {
        modelDir = modelLogical.substr(0, pos + 1);
        for (auto& c : modelDir) if (c == '\\') c = '/';
    }
    std::string filename = norm;
    if (auto pos = norm.find_last_of('/'); pos != std::string::npos)
        filename = norm.substr(pos + 1);

    std::vector<std::string> candidates;
    if (norm.find('/') != std::string::npos) candidates.push_back(norm);
    candidates.push_back(modelDir + norm);
    candidates.push_back(modelDir + filename);
    candidates.push_back(modelDir + "Textures/" + filename);
    candidates.push_back("Textures/" + filename);
    candidates.push_back(filename);

    std::unordered_set<std::string> seen;
    std::vector<std::string> uniq;
    for (auto& c : candidates) {
        auto n = MM_NormalizePath(c);
        if (seen.insert(n).second) uniq.push_back(n);
    }
    for (auto& c : uniq) {
        if (AssetManager::Instance()->Exists(c)) {
            return c;
        }
    }
	ErrorLogger::Instance().LogError("ModelManager", "Texture not found: " + rawPath + " (tried " + std::to_string(uniq.size()) + " paths)", false, 3);
    return {};
}

void ModelManager::ProcessNode(aiNode* node, const aiScene* scene,
    std::vector<ModelVertex>& vertices,
    std::vector<uint32_t>& indices,
    std::shared_ptr<ModelSharedResource> shared) {

    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene, vertices, indices, shared);
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, vertices, indices, shared);
    }
}

void ModelManager::ProcessMesh(aiMesh* mesh, const aiScene* scene,
    std::vector<ModelVertex>& vertices,
    std::vector<uint32_t>& indices,
    std::shared_ptr<ModelSharedResource> shared) {

    SubMesh subMesh;
    subMesh.indexOffset = static_cast<uint32_t>(indices.size());
    subMesh.materialIndex = mesh->mMaterialIndex;
    subMesh.skinned = mesh->HasBones();

    uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());

    // ========================================
    // 1. UV チャンネルの決定
    // ========================================
    unsigned useUVChannel = 0;
    if (mesh->GetNumUVChannels() > 1) {
        float bestArea = -1.0f;
        for (unsigned ch = 0; ch < mesh->GetNumUVChannels(); ++ch) {
            if (!mesh->mTextureCoords[ch]) continue;
            float minU = 1e9f, maxU = -1e9f, minV = 1e9f, maxV = -1e9f;
            for (uint32_t vi = 0; vi < mesh->mNumVertices; ++vi) {
                auto& uv = mesh->mTextureCoords[ch][vi];
                minU = std::min(minU, uv.x);
                maxU = std::max(maxU, uv.x);
                minV = std::min(minV, uv.y);
                maxV = std::max(maxV, uv.y);
            }
            float du = maxU - minU;
            float dv = maxV - minV;
            float area = du * dv;
            if (area > bestArea) {
                bestArea = area;
                useUVChannel = ch;
            }
        }
        char dbg[128];
        sprintf_s(dbg, "[ModelManager] Mesh mat=%u select UV channel=%u\n",
            mesh->mMaterialIndex, useUVChannel);
        OutputDebugStringA(dbg);
    }

    // ========================================
    // 2. ボーンマッピングの構築（メッシュ全体で1回だけ）
    // ========================================
    std::map<std::string, int> boneMap;
    for (size_t i = 0; i < shared->bones.size(); ++i) {
        boneMap[shared->bones[i].name] = static_cast<int>(i);
    }

    // ========================================
    // 3. ボーンウェイト用の一時バッファ
    // ========================================
    struct VertexBoneData {
        std::vector<std::pair<int, float>> weights; // (boneIndex, weight)
    };
    std::vector<VertexBoneData> vertexBoneData(mesh->mNumVertices);

    // ========================================
    // 4. ボーンウェイトの収集（メッシュ全体で1回だけ）
    // ========================================
    if (mesh->HasBones()) {
        for (uint32_t boneIdx = 0; boneIdx < mesh->mNumBones; boneIdx++) {
            aiBone* bone = mesh->mBones[boneIdx];
            std::string boneName = bone->mName.C_Str();

            int boneIndex = -1;
            auto it = boneMap.find(boneName);
            if (it != boneMap.end()) {
                boneIndex = it->second;
            }
            else {
                // 新しいボーンを追加
                boneIndex = static_cast<int>(shared->bones.size());
                boneMap[boneName] = boneIndex;

                Bone newBone;
                newBone.name = boneName;
                newBone.parentIndex = -1;

                aiMatrix4x4& m = bone->mOffsetMatrix;
                newBone.offset = DirectX::XMMATRIX(
                    m.a1, m.b1, m.c1, m.d1,
                    m.a2, m.b2, m.c2, m.d2,
                    m.a3, m.b3, m.c3, m.d3,
                    m.a4, m.b4, m.c4, m.d4
                );

                shared->bones.push_back(newBone);
            }

            // 各頂点にウェイトを設定
            for (uint32_t weightIdx = 0; weightIdx < bone->mNumWeights; weightIdx++) {
                uint32_t vertexId = bone->mWeights[weightIdx].mVertexId;
                float weight = bone->mWeights[weightIdx].mWeight;

                if (vertexId < vertexBoneData.size()) {
                    vertexBoneData[vertexId].weights.push_back({ boneIndex, weight });
                }
            }
        }
    }

    // ========================================
    // 5. 頂点データの構築（ボーンウェイトを含む）
    // ========================================
    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        ModelVertex vertex{};

        // 位置
        vertex.position[0] = mesh->mVertices[i].x;
        vertex.position[1] = mesh->mVertices[i].y;
        vertex.position[2] = mesh->mVertices[i].z;

        // 法線
        if (mesh->HasNormals()) {
            vertex.normal[0] = mesh->mNormals[i].x;
            vertex.normal[1] = mesh->mNormals[i].y;
            vertex.normal[2] = mesh->mNormals[i].z;
        }

        // タンジェント
        if (mesh->mTangents) {
            vertex.tangent[0] = mesh->mTangents[i].x;
            vertex.tangent[1] = mesh->mTangents[i].y;
            vertex.tangent[2] = mesh->mTangents[i].z;
            vertex.tangent[3] = 1.0f;
        }

        // UV座標
        if (mesh->mTextureCoords[useUVChannel]) {
            vertex.uv[0] = mesh->mTextureCoords[useUVChannel][i].x;
            vertex.uv[1] = mesh->mTextureCoords[useUVChannel][i].y;
        }

        // ⭐ ボーンウェイトの設定
        for (int j = 0; j < 4; j++) {
            vertex.boneIndices[j] = 0;
            vertex.boneWeights[j] = 0.0f;
        }

        if (i < vertexBoneData.size() && !vertexBoneData[i].weights.empty()) {
            auto& boneData = vertexBoneData[i];

            // ウェイトを降順にソート
            std::sort(boneData.weights.begin(), boneData.weights.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });

            // 上位4つのウェイトを設定
            float totalWeight = 0.0f;
            int count = std::min(4, static_cast<int>(boneData.weights.size()));
            for (int j = 0; j < count; j++) {
                vertex.boneIndices[j] = boneData.weights[j].first;
                vertex.boneWeights[j] = boneData.weights[j].second;
                totalWeight += boneData.weights[j].second;
            }

            // ウェイトの正規化
            if (totalWeight > 0.0f && totalWeight != 1.0f) {
                for (int j = 0; j < count; j++) {
                    vertex.boneWeights[j] /= totalWeight;
                }
            }
        }

        vertices.push_back(vertex);
    }

    // ========================================
    // 6. インデックスの構築
    // ========================================
    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(vertexOffset + face.mIndices[j]);
        }
    }

    // ========================================
    // 7. UV検証
    // ========================================
    bool anyUV = false;
    bool allZero = true;
    if (mesh->mTextureCoords[0]) {
        anyUV = true;
        for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
            float ux = mesh->mTextureCoords[0][i].x;
            float uy = mesh->mTextureCoords[0][i].y;
            if (!(ux == 0.0f && uy == 0.0f)) {
                allZero = false;
                break;
            }
        }
    }

    subMesh.hasUV = anyUV;
    subMesh.uvAllZero = anyUV ? allZero : false;

    if (!anyUV) {
        ErrorLogger::Instance().LogError("ModelManager",
            "[Mesh mat=" + std::to_string(mesh->mMaterialIndex) + "] UV channel MISSING", false, 3);
    }
    else if (allZero) {
        ErrorLogger::Instance().LogError("ModelManager",
            "[Mesh mat=" + std::to_string(mesh->mMaterialIndex) + "] UV ALL ZERO", false, 3);
    }

    // ========================================
    // 8. SubMeshの登録
    // ========================================
    subMesh.indexCount = static_cast<uint32_t>(indices.size()) - subMesh.indexOffset;
    shared->submeshes.push_back(subMesh);

    // ⭐ デバッグ出力
    if (mesh->HasBones()) {
        char dbg[256];
        sprintf_s(dbg, "[ModelManager] Mesh mat=%u: %u vertices, %u bones processed\n",
            mesh->mMaterialIndex, mesh->mNumVertices, mesh->mNumBones);
        OutputDebugStringA(dbg);
    }
}

bool ModelManager::CreateGPUBuffers(const std::vector<ModelVertex>& vertices,
    const std::vector<uint32_t>& indices,
    std::shared_ptr<ModelSharedResource> shared) {

    auto device = DirectX11::GetInstance()->GetDevice();
    if (!device) return false;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(ModelVertex));
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, shared->vb.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    hr = device->CreateBuffer(&ibDesc, &ibData, shared->ib.GetAddressOf());
    if (FAILED(hr)) return false;

    shared->vertexCount = static_cast<uint32_t>(vertices.size());
    shared->indexCount = static_cast<uint32_t>(indices.size());

    return true;
}

void ModelManager::ProcessMaterials(const aiScene* scene, std::shared_ptr<ModelSharedResource> shared) {
    for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* mat = scene->mMaterials[i];
        MaterialShared material;

        aiColor4D color;
        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
            material.baseColor = DirectX::XMFLOAT4(color.r, color.g, color.b, color.a);
        }

        float metallic, roughness;
        if (AI_SUCCESS == mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic)) material.metallic = metallic;
        if (AI_SUCCESS == mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness)) material.roughness = roughness;

        aiString texPath;
        // 複数のテクスチャタイプを優先順で試行 (DIFFUSE → BASE_COLOR → EMISSIVE → AMBIENT)
        aiTextureType texTypes[] = { 
            aiTextureType_DIFFUSE, 
            aiTextureType_BASE_COLOR, 
            aiTextureType_EMISSIVE, 
            aiTextureType_AMBIENT 
        };
        
        bool foundTexture = false;
        for (aiTextureType texType : texTypes) {
            if (AI_SUCCESS == mat->GetTexture(texType, 0, &texPath)) {
                std::string resolved = ResolveTexturePath(shared->source, texPath.C_Str());
                material.baseColorTex = resolved;
                foundTexture = true;
                // デバッグ用: どのテクスチャタイプで見つかったかログ出力
                OutputDebugStringA(("[ModelManager] Material " + std::to_string(i) + 
                    " texture found in type " + std::to_string(texType) + 
                    ": " + resolved + "\n").c_str());
                break; // 最初に見つかったテクスチャを使用
            }
        }
        
        if (!foundTexture) {
            OutputDebugStringA(("[ModelManager] Material " + std::to_string(i) + 
                " has no texture in any supported type\n").c_str());
        }

        shared->materials.push_back(material);
    }
}

void ModelManager::ProcessBones(const aiScene* scene, std::shared_ptr<ModelSharedResource> shared) {
    shared->hasSkin = false;

    // ボーンデータを収集
    std::map<std::string, int> boneMap;
    std::vector<ModelVertex>* verticesPtr = nullptr; // 実際の頂点データへの参照が必要

    for (uint32_t meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++) {
        aiMesh* mesh = scene->mMeshes[meshIdx];

        if (!mesh->HasBones()) continue;

        shared->hasSkin = true;

        for (uint32_t boneIdx = 0; boneIdx < mesh->mNumBones; boneIdx++) {
            aiBone* bone = mesh->mBones[boneIdx];
            std::string boneName = bone->mName.C_Str();

            int boneIndex = -1;
            auto it = boneMap.find(boneName);
            if (it == boneMap.end()) {
                // 新しいボーンを追加
                boneIndex = static_cast<int>(shared->bones.size());
                boneMap[boneName] = boneIndex;

                Bone newBone;
                newBone.name = boneName;
                newBone.parentIndex = -1; // 後で階層を構築

                // オフセット行列の変換
                aiMatrix4x4& m = bone->mOffsetMatrix;
                newBone.offset = DirectX::XMMATRIX(
                    m.a1, m.b1, m.c1, m.d1,
                    m.a2, m.b2, m.c2, m.d2,
                    m.a3, m.b3, m.c3, m.d3,
                    m.a4, m.b4, m.c4, m.d4
                );

                shared->bones.push_back(newBone);
            }
            else {
                boneIndex = it->second;
            }

            // ⭐ ボーンウェイトの適用（これが重要！）
            // 注意: この時点では頂点データへのアクセス方法を検討する必要があります
            // 現在の実装では頂点データが ProcessMesh で処理されているため、
            // ボーンウェイトの適用は ProcessMesh 内で行う必要があります
        }
    }

    // デバッグ出力
    if (shared->hasSkin) {
        OutputDebugStringA(("[ModelManager] Loaded " +
            std::to_string(shared->bones.size()) + " bones\n").c_str());
    }
}

void ModelManager::ProcessAnimations(const aiScene* scene, std::shared_ptr<ModelSharedResource> shared) {
    if (!scene->HasAnimations()) return;

    for (uint32_t i = 0; i < scene->mNumAnimations; i++) {
        aiAnimation* anim = scene->mAnimations[i];
        AnimationClip clip;

        clip.name = anim->mName.length > 0 ? anim->mName.C_Str() : ("Animation_" + std::to_string(i));
        clip.duration = anim->mDuration;
        clip.tps = anim->mTicksPerSecond != 0.0 ? anim->mTicksPerSecond : 25.0;

        // === ⭐ 修正：ノード階層の構築（クリップごとに） ===
        std::map<std::string, int> nodeNameToIndex;
        BuildNodeHierarchy(scene->mRootNode, clip, nodeNameToIndex, -1);

        // チャンネルの処理
        for (uint32_t ch = 0; ch < anim->mNumChannels; ch++) {
            aiNodeAnim* nodeAnim = anim->mChannels[ch];
            AnimationChannel channel;

            channel.nodeName = nodeAnim->mNodeName.C_Str();
            auto it = nodeNameToIndex.find(channel.nodeName);
            channel.nodeIndex = (it != nodeNameToIndex.end()) ? it->second : -1;

            // Position keys
            for (uint32_t k = 0; k < nodeAnim->mNumPositionKeys; k++) {
                float time = static_cast<float>(nodeAnim->mPositionKeys[k].mTime / clip.tps);
                DirectX::XMFLOAT3 pos(
                    nodeAnim->mPositionKeys[k].mValue.x,
                    nodeAnim->mPositionKeys[k].mValue.y,
                    nodeAnim->mPositionKeys[k].mValue.z
                );
                channel.positionKeys.push_back({ time, pos });
            }

            // Rotation keys
            for (uint32_t k = 0; k < nodeAnim->mNumRotationKeys; k++) {
                float time = static_cast<float>(nodeAnim->mRotationKeys[k].mTime / clip.tps);
                DirectX::XMFLOAT4 rot(
                    nodeAnim->mRotationKeys[k].mValue.x,
                    nodeAnim->mRotationKeys[k].mValue.y,
                    nodeAnim->mRotationKeys[k].mValue.z,
                    nodeAnim->mRotationKeys[k].mValue.w
                );
                channel.rotationKeys.push_back({ time, rot });
            }

            // Scale keys
            for (uint32_t k = 0; k < nodeAnim->mNumScalingKeys; k++) {
                float time = static_cast<float>(nodeAnim->mScalingKeys[k].mTime / clip.tps);
                DirectX::XMFLOAT3 scale(
                    nodeAnim->mScalingKeys[k].mValue.x,
                    nodeAnim->mScalingKeys[k].mValue.y,
                    nodeAnim->mScalingKeys[k].mValue.z
                );
                channel.scaleKeys.push_back({ time, scale });
            }

            clip.channels.push_back(channel);
        }

        shared->clips.push_back(clip);
    }
}

void ModelManager::BuildNodeHierarchy(
    aiNode* node,
    AnimationClip& clip,
    std::map<std::string, int>& nodeNameToIndex,
    int parentIndex)
{
    AnimationClip::NodeInfo nodeInfo;
    nodeInfo.name = node->mName.C_Str();
    nodeInfo.parentIndex = parentIndex;

    // ローカル変換行列の取得
    aiMatrix4x4& t = node->mTransformation;
    nodeInfo.localTransform = DirectX::XMMATRIX(
        t.a1, t.b1, t.c1, t.d1,
        t.a2, t.b2, t.c2, t.d2,
        t.a3, t.b3, t.c3, t.d3,
        t.a4, t.b4, t.c4, t.d4
    );

    int currentIndex = static_cast<int>(clip.nodeHierarchy.size());
    nodeNameToIndex[nodeInfo.name] = currentIndex;

    // 子ノードのインデックスを予約
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        nodeInfo.children.push_back(currentIndex + 1 + i);
    }

    clip.nodeHierarchy.push_back(nodeInfo);

    // 再帰的に子ノードを処理
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        BuildNodeHierarchy(node->mChildren[i], clip, nodeNameToIndex, currentIndex);
    }
}

void ModelManager::GarbageCollect() {
    std::lock_guard<std::mutex> lk(m_mtx);
    for (auto it = m_cache.begin(); it != m_cache.end(); ) {
        if (it->second.weak.expired()) {
            it = m_cache.erase(it);
        }
        else {
            ++it;
        }
    }
}

void ModelManager::DrawDebugGUI() {
    std::lock_guard<std::mutex> lk(m_mtx);
    ImGui::TextUnformatted("ModelManager");
    ImGui::Separator();
    size_t alive = 0;
    size_t totalGPU = 0;
    for (auto& kv : m_cache) {
        if (!kv.second.weak.expired()) {
            alive++;
            totalGPU += kv.second.gpuBytes;
        }
    }
    ImGui::Text("Cached: %zu (alive=%zu)", m_cache.size(), alive);
    ImGui::Text("GPU Approx Total: %.2f MB", totalGPU / (1024.0 * 1024.0));
    static char filter[128] = "";
    ImGui::InputText("Filter##Model", filter, sizeof(filter));
    if (ImGui::Button("GC Dead")) {
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            if (it->second.weak.expired()) it = m_cache.erase(it);
            else ++it;
        }
    }
    ImGui::Separator();
    ImGui::BeginChild("ModelList", ImVec2(0, 160), true);
    for (auto& kv : m_cache) {
        if (filter[0] && kv.first.find(filter) == std::string::npos) continue;
        bool aliveRes = !kv.second.weak.expired();
        ImGui::Text("%s | %s | %.2f KB | lastUse=%llu",
            kv.first.c_str(),
            aliveRes ? "alive" : "dead",
            kv.second.gpuBytes / 1024.0,
            (unsigned long long)kv.second.lastUse);
    }
    ImGui::EndChild();
}