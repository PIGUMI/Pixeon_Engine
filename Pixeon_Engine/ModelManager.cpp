#define NOMINMAX
#include "ModelManager.h"
#include "AssetManager.h"
#include "System.h"
#include "ErrorLog.h"
#include "EditrGUI.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Windows.h>
#include "IMGUI/imgui.h"
#include <filesystem>
#include <unordered_set>
#include <algorithm>
#include "BoneNodeMapping.h"
#include <functional>

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

ModelManager* ModelManager::s_instance = nullptr;

static std::string MM_NormalizePath(std::string s) {
    for (auto& c : s) if (c == '\\') c = '/';
    while (s.size() && (s[0] == '/' || (s.size() >= 2 && s.rfind("./", 0) == 0))) {
        if (s[0] == '/') s.erase(0, 1);
        else if (s.rfind("./", 0) == 0) s.erase(0, 2);
        else break;
    }
    return s;
}

ModelManager* ModelManager::Instance() {
    if (!s_instance) s_instance = new ModelManager();
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
        Entry e; e.weak = res; e.lastUse = m_frame; e.gpuBytes = res->gpuBytes;
        m_cache[logicalName] = e;
    }
    return res;
}

std::shared_ptr<ModelSharedResource> ModelManager::LoadInternal(const std::string& logicalName) {
    std::vector<uint8_t> data;
    if (!AssetManager::Instance()->LoadAsset(logicalName, data) || data.empty()) {
        ErrorLogger::Instance().LogError("ModelManager", "Failed load asset: " + logicalName);
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

    auto device = DirectX11::GetInstance()->GetDevice();
    if (!device) {
        ErrorLogger::Instance().LogError("ModelManager", "Device null");
        return nullptr;
    }
    { // VB
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = (UINT)(vertices.size() * sizeof(ModelVertex));
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA srd{ vertices.data(),0,0 };
        if (FAILED(device->CreateBuffer(&bd, &srd, shared->vb.GetAddressOf()))) {
            ErrorLogger::Instance().LogError("ModelManager", "VB creation failed");
            return nullptr;
        }
    }
    { // IB
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = (UINT)(indices.size() * sizeof(uint32_t));
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA srd{ indices.data(),0,0 };
        if (FAILED(device->CreateBuffer(&bd, &srd, shared->ib.GetAddressOf()))) {
            ErrorLogger::Instance().LogError("ModelManager", "IB creation failed");
            return nullptr;
        }
    }
    shared->vertexCount = (uint32_t)vertices.size();
    shared->indexCount = (uint32_t)indices.size();
    shared->gpuBytes = vertices.size() * sizeof(ModelVertex) + indices.size() * sizeof(uint32_t);

    ProcessMaterials(scene, shared);
    ProcessBonesFinalizeHierarchy(scene, shared);
    ProcessAnimations(scene, shared);

    return shared;
}

void ModelManager::ProcessNode(aiNode* node, const aiScene* scene,
    std::vector<ModelVertex>& vertices,
    std::vector<uint32_t>& indices,
    std::shared_ptr<ModelSharedResource> shared) {
    for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene, vertices, indices, shared);
    }
    for (uint32_t i = 0; i < node->mNumChildren; ++i)
        ProcessNode(node->mChildren[i], scene, vertices, indices, shared);
}

void ModelManager::ProcessMesh(aiMesh* mesh, const aiScene* scene,
    std::vector<ModelVertex>& vertices,
    std::vector<uint32_t>& indices,
    std::shared_ptr<ModelSharedResource> shared)
{
    SubMesh sm;
    sm.indexOffset = (uint32_t)indices.size();
    sm.materialIndex = mesh->mMaterialIndex;
    sm.skinned = mesh->HasBones();
    uint32_t vtxOffset = (uint32_t)vertices.size();

    // 既存ボーン名 -> インデックス
    std::unordered_map<std::string, int> boneMap;
    for (size_t i = 0; i < shared->bones.size(); ++i)
        boneMap[shared->bones[i].name] = (int)i;

    struct TmpWeight { std::vector<std::pair<int, float>> w; };
    std::vector<TmpWeight> tmp(mesh->mNumVertices);

    if (mesh->HasBones()) {
        for (uint32_t bi = 0; bi < mesh->mNumBones; ++bi) {
            aiBone* ab = mesh->mBones[bi];
            std::string bname = ab->mName.C_Str();
            int boneIndex = -1;
            auto it = boneMap.find(bname);
            if (it == boneMap.end()) {
                boneIndex = (int)shared->bones.size();
                boneMap[bname] = boneIndex;
                Bone newBone;
                newBone.name = bname;
                // InverseBindPose
                aiMatrix4x4& m = ab->mOffsetMatrix;
                newBone.offset = DirectX::XMMatrixSet(
                    m.a1, m.b1, m.c1, m.d1,
                    m.a2, m.b2, m.c2, m.d2,
                    m.a3, m.b3, m.c3, m.d3,
                    m.a4, m.b4, m.c4, m.d4
                );
                // 必要ならスケール/反転（現在は loadScale=1, zFlip=false）
                float loadScale = 1.0f;
                bool  zFlip = false;
                if (loadScale != 1.0f) {
                    newBone.offset.r[3].m128_f32[0] *= loadScale;
                    newBone.offset.r[3].m128_f32[1] *= loadScale;
                    newBone.offset.r[3].m128_f32[2] *= loadScale;
                }
                if (zFlip) {
                    DirectX::XMMATRIX flip = DirectX::XMMatrixScaling(-1.f, 1.f, 1.f);
                    newBone.offset = flip * newBone.offset;
                }
                newBone.invOffset = DirectX::XMMatrixInverse(nullptr, newBone.offset);
                newBone.parentIndex = -1;
                newBone.nodeIndex = -1; // クリップ階層生成後に確定
                shared->bones.push_back(newBone);
            }
            else {
                boneIndex = it->second;
            }
            for (uint32_t w = 0; w < ab->mNumWeights; ++w) {
                uint32_t vid = ab->mWeights[w].mVertexId;
                float     val = ab->mWeights[w].mWeight;
                if (vid < tmp.size())
                    tmp[vid].w.push_back({ boneIndex, val });
            }
        }
    }

    // 頂点生成
    for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
        ModelVertex mv{};
        mv.position[0] = mesh->mVertices[v].x;
        mv.position[1] = mesh->mVertices[v].y;
        mv.position[2] = mesh->mVertices[v].z;
        if (mesh->HasNormals()) {
            mv.normal[0] = mesh->mNormals[v].x;
            mv.normal[1] = mesh->mNormals[v].y;
            mv.normal[2] = mesh->mNormals[v].z;
        }
        if (mesh->mTangents) {
            mv.tangent[0] = mesh->mTangents[v].x;
            mv.tangent[1] = mesh->mTangents[v].y;
            mv.tangent[2] = mesh->mTangents[v].z;
            mv.tangent[3] = 1.0f;
        }
        if (mesh->mTextureCoords[0]) {
            mv.uv[0] = mesh->mTextureCoords[0][v].x;
            mv.uv[1] = mesh->mTextureCoords[0][v].y;
        }
        for (int k = 0; k < 4; ++k) { mv.boneIndices[k] = 0; mv.boneWeights[k] = 0.0f; }

        if (!tmp[v].w.empty()) {
            auto& arr = tmp[v].w;
            std::sort(arr.begin(), arr.end(), [](auto& a, auto& b) {return a.second > b.second; });
            int count = std::min<int>(4, (int)arr.size());
            float total = 0.f;
            for (int k = 0; k < count; ++k) {
                mv.boneIndices[k] = arr[k].first;
                mv.boneWeights[k] = arr[k].second;
                total += arr[k].second;
            }
            if (total > 0.f && fabs(total - 1.f) > 1e-5f) {
                for (int k = 0; k < count; ++k)
                    mv.boneWeights[k] /= total;
            }
        }
        vertices.push_back(mv);
    }

    // インデックス
    for (uint32_t f = 0; f < mesh->mNumFaces; ++f) {
        aiFace face = mesh->mFaces[f];
        for (uint32_t j = 0; j < face.mNumIndices; ++j)
            indices.push_back(vtxOffset + face.mIndices[j]);
    }

    // UV状況
    bool anyUV = mesh->mTextureCoords[0] != nullptr;
    bool allZero = true;
    if (anyUV) {
        for (uint32_t vv = 0; vv < mesh->mNumVertices; ++vv) {
            float ux = mesh->mTextureCoords[0][vv].x;
            float uy = mesh->mTextureCoords[0][vv].y;
            if (ux != 0.f || uy != 0.f) { allZero = false; break; }
        }
    }
    sm.hasUV = anyUV;
    sm.uvAllZero = anyUV ? allZero : false;
    sm.indexCount = (uint32_t)indices.size() - sm.indexOffset;
    shared->submeshes.push_back(sm);
}

void ModelManager::ProcessMaterials(const aiScene* scene,
    std::shared_ptr<ModelSharedResource> shared) {
    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* mat = scene->mMaterials[i];
        MaterialShared ms;
        aiColor4D col;
        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, col))
            ms.baseColor = { col.r,col.g,col.b,col.a };
        aiString texPath;
        if (AI_SUCCESS == mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath)) {
            ms.baseColorTex = ResolveTexturePath(shared->source, texPath.C_Str());
        }
        shared->materials.push_back(ms);
    }
}

// ★ これが不足していたためリンクエラー
std::string ModelManager::ResolveTexturePath(const std::string& modelLogical, const std::string& rawPath) {
    if (rawPath.empty()) return {};
    if (rawPath[0] == '*') {
        ErrorLogger::Instance().LogError("ModelManager", "Embedded texture unsupported: " + rawPath);
        return {};
    }
    std::string norm = MM_NormalizePath(rawPath);

    // 絶対パスならファイル名のみ (Assimp が絶対パスを吐くケースの簡易対処)
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
    // 見つからない場合はログのみ
    ErrorLogger::Instance().LogError("ModelManager", "Texture not found: " + rawPath + " (tried " + std::to_string(uniq.size()) + " paths)", false, 3);
    return {};
}

// 修正後: ProcessBonesFinalizeHierarchy
void ModelManager::ProcessBonesFinalizeHierarchy(const aiScene* scene,
    std::shared_ptr<ModelSharedResource> shared)
{
    if (shared->bones.empty()) return;

    // 親ノード名マップ（aiNode 再帰）
    std::unordered_map<std::string, std::string> parentName;
    std::function<void(aiNode*, aiNode*)> walk = [&](aiNode* n, aiNode* p) {
        if (!n) return;
        parentName[n->mName.C_Str()] = p ? p->mName.C_Str() : "";
        for (uint32_t c = 0; c < n->mNumChildren; ++c)
            walk(n->mChildren[c], n);
        };
    walk(scene->mRootNode, nullptr);

    // Bone 名→インデックス
    std::unordered_map<std::string, int> boneIndexMap;
    for (int i = 0; i < (int)shared->bones.size(); ++i)
        boneIndexMap[shared->bones[i].name] = i;

    // 親インデックス設定
    for (auto& b : shared->bones) {
        auto pit = parentName.find(b.name);
        if (pit == parentName.end() || pit->second.empty()) {
            b.parentIndex = -1;
            continue;
        }
        auto bit = boneIndexMap.find(pit->second);
        b.parentIndex = (bit != boneIndexMap.end()) ? bit->second : -1;
    }

    // nodeIndex 設定: AnimationClip が生成済みなら最初のクリップから名前マップ
    if (!shared->clips.empty()) {
        // すべてのクリップで同じ階層前提なら 0 番で十分
        const auto& clip = shared->clips[0];
        std::unordered_map<std::string, int> nodeNameToIndex;
        for (int i = 0; i < (int)clip.nodeHierarchy.size(); ++i)
            nodeNameToIndex[clip.nodeHierarchy[i].name] = i;

        for (auto& b : shared->bones) {
            auto it = nodeNameToIndex.find(b.name);
            if (it != nodeNameToIndex.end())
                b.nodeIndex = it->second;
            else
                b.nodeIndex = -1;
        }
    }
}

// --- 修正対象: ProcessAnimations （末尾に MapBonesToNodes 呼び出しを追加） ---
void ModelManager::ProcessAnimations(const aiScene* scene,
    std::shared_ptr<ModelSharedResource> shared)
{
    if (!scene->HasAnimations()) return;
    for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
        aiAnimation* anim = scene->mAnimations[i];
        AnimationClip clip;
        clip.name = anim->mName.length ? anim->mName.C_Str() : ("Animation_" + std::to_string(i));
        clip.duration = anim->mDuration;
        clip.tps = anim->mTicksPerSecond != 0.0 ? anim->mTicksPerSecond : 25.0;

        std::map<std::string, int> nodeMap;
        BuildNodeHierarchy(scene->mRootNode, clip, nodeMap, -1);

        for (uint32_t ch = 0; ch < anim->mNumChannels; ++ch) {
            aiNodeAnim* na = anim->mChannels[ch];
            AnimationChannel ac;
            ac.nodeName = na->mNodeName.C_Str();
            auto it = nodeMap.find(ac.nodeName);
            ac.nodeIndex = (it != nodeMap.end()) ? it->second : -1;
            // position keys
            for (uint32_t k = 0; k < na->mNumPositionKeys; ++k) {
                float t = (float)(na->mPositionKeys[k].mTime / clip.tps);
                ac.positionKeys.push_back({ t,{
                    na->mPositionKeys[k].mValue.x,
                    na->mPositionKeys[k].mValue.y,
                    na->mPositionKeys[k].mValue.z } });
            }
            // rotation keys
            for (uint32_t k = 0; k < na->mNumRotationKeys; ++k) {
                float t = (float)(na->mRotationKeys[k].mTime / clip.tps);
                ac.rotationKeys.push_back({ t,{
                    na->mRotationKeys[k].mValue.x,
                    na->mRotationKeys[k].mValue.y,
                    na->mRotationKeys[k].mValue.z,
                    na->mRotationKeys[k].mValue.w } });
            }
            // scale keys
            for (uint32_t k = 0; k < na->mNumScalingKeys; ++k) {
                float t = (float)(na->mScalingKeys[k].mTime / clip.tps);
                ac.scaleKeys.push_back({ t,{
                    na->mScalingKeys[k].mValue.x,
                    na->mScalingKeys[k].mValue.y,
                    na->mScalingKeys[k].mValue.z } });
            }
            clip.channels.push_back(ac);
        }
        shared->clips.push_back(clip);
    }

    // ★ 追加: クリップ階層が揃った後で bone.nodeIndex を一括確定
    MapBonesToNodes(*shared);
}

void ModelManager::BuildNodeHierarchy(aiNode* node,
    AnimationClip& clip,
    std::map<std::string, int>& nodeNameToIndex,
    int parentIndex) {
    AnimationClip::NodeInfo ni;
    ni.name = node->mName.C_Str();
    ni.parentIndex = parentIndex;
    aiMatrix4x4& t = node->mTransformation;
    ni.localTransform = DirectX::XMMatrixSet(
        t.a1, t.b1, t.c1, t.d1,
        t.a2, t.b2, t.c2, t.d2,
        t.a3, t.b3, t.c3, t.d3,
        t.a4, t.b4, t.c4, t.d4
    );
    int current = (int)clip.nodeHierarchy.size();
    nodeNameToIndex[ni.name] = current;
    clip.nodeHierarchy.push_back(ni);
    for (uint32_t i = 0; i < node->mNumChildren; ++i) {
        int childIndex = (int)clip.nodeHierarchy.size();
        clip.nodeHierarchy[current].children.push_back(childIndex);
        BuildNodeHierarchy(node->mChildren[i], clip, nodeNameToIndex, current);
    }
}

void ModelManager::GarbageCollect() {
    std::lock_guard<std::mutex> lk(m_mtx);
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (it->second.weak.expired()) it = m_cache.erase(it);
        else ++it;
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