#define NOMINMAX
#include "ModelManager.h"
#include "EditrGUI.h"
#include "ErrorLog.h"
#include "System.h"
#include <unordered_map>
#include <algorithm>

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

using namespace DirectX;

ModelManagerRebuild* ModelManagerRebuild::s_inst = nullptr;

ModelManagerRebuild* ModelManagerRebuild::Instance() {
    if (!s_inst) s_inst = new ModelManagerRebuild();
    return s_inst;
}

std::shared_ptr<ModelSharedResourceNew>
ModelManagerRebuild::Load(const std::string& logicalName) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(logicalName,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenNormals |
        aiProcess_LimitBoneWeights |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs);

    if (!scene || !scene->mRootNode) {
        //ErrorLog::Instance().LogError("ModelManager", "Assimp load failed: " + logicalName);
		EditrGUI::GetInstance()->WriteLog("[Load] Assimp load failed: " + logicalName);
        return nullptr;
    }

    auto res = std::make_shared<ModelSharedResourceNew>();
    res->source = logicalName;

    // 1. ノード階層構築
    BuildNodeHierarchy(scene->mRootNode, -1, res->nodes);

    // 2. メッシュ + ボーン
    std::vector<ModelVertexNew> vertices;
    std::vector<uint32_t> indices;
    for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
        ProcessMesh(scene->mMeshes[i], scene,
            vertices, indices, res->submeshes,
            res->bones, res->nodes);
    }

    // 3. アニメーション
    ProcessAnimations(scene, res->clips, res->nodes);

    // 4. ボーン親インデックス確定
    FinalizeBoneParents(res->bones);

    // 5. 骨 nodeIndex 未解決チェック
    int missing = 0;
    for (auto& b : res->bones) if (b.nodeIndex < 0) ++missing;
    EditrGUI::GetInstance()->WriteLog("[Load] bones=" + std::to_string(res->bones.size()) +
        " missingNodeIndex=" + std::to_string(missing));

    // 6. GPUバッファ作成
    auto dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) return nullptr;
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = (UINT)(vertices.size() * sizeof(ModelVertexNew));
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA srd{ vertices.data(),0,0 };
        if (FAILED(dev->CreateBuffer(&bd, &srd, res->vb.GetAddressOf()))) {
			EditrGUI::GetInstance()->WriteLog("[Load] VB create fail");
            return nullptr;
        }
    }
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = (UINT)(indices.size() * sizeof(uint32_t));
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA srd{ indices.data(),0,0 };
        if (FAILED(dev->CreateBuffer(&bd, &srd, res->ib.GetAddressOf()))) {
			EditrGUI::GetInstance()->WriteLog("[Load] IB create fail");
            return nullptr;
        }
    }
    res->vertexCount = (uint32_t)vertices.size();
    res->indexCount = (uint32_t)indices.size();

    // 7. マテリアル
    for (unsigned m = 0; m < scene->mNumMaterials; ++m) {
        aiMaterial* mat = scene->mMaterials[m];
        MaterialNew mn;
        aiColor4D col;
        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, col))
            mn.baseColor = { col.r,col.g,col.b,col.a };
        else
            mn.baseColor = { 1,1,1,1 };
        aiString path;
        if (AI_SUCCESS == mat->GetTexture(aiTextureType_DIFFUSE, 0, &path))
            mn.baseColorTex = path.C_Str();
        res->materials.push_back(mn);
    }

    return res;
}

void ModelManagerRebuild::BuildNodeHierarchy(aiNode* node, int parent, std::vector<NodeInfo>& nodes) {
    NodeInfo ni;
    ni.name = node->mName.C_Str();
    ni.parentIndex = parent;
    // bindLocal: assimp の node->mTransformation は bind pose
    aiMatrix4x4& t = node->mTransformation;
    ni.bindLocal = XMMatrixSet(
        t.a1, t.b1, t.c1, t.d1,
        t.a2, t.b2, t.c2, t.d2,
        t.a3, t.b3, t.c3, t.d3,
        t.a4, t.b4, t.c4, t.d4
    );
    int current = (int)nodes.size();
    nodes.push_back(ni);
    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        int childIndex = (int)nodes.size();
        nodes[current].children.push_back(childIndex);
        BuildNodeHierarchy(node->mChildren[i], current, nodes);
    }
}

int ModelManagerRebuild::FindNodeIndex(const std::vector<NodeInfo>& nodes, const std::string& name) {
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].name == name) return (int)i;
    }
    return -1;
}

void ModelManagerRebuild::ProcessMesh(aiMesh* mesh, const aiScene* scene,
    std::vector<ModelVertexNew>& vertices,
    std::vector<uint32_t>& indices,
    std::vector<SubMeshNew>& submeshes,
    std::vector<BoneInfo>& bones,
    const std::vector<NodeInfo>& nodes)
{
    SubMeshNew sm{};
    sm.indexOffset = (uint32_t)indices.size();
    sm.materialIndex = mesh->mMaterialIndex;
    sm.skinned = mesh->HasBones();
    uint32_t vtxBase = (uint32_t)vertices.size();

    // 既存ボーン名→インデックス
    std::unordered_map<std::string, int> boneMap;
    for (int i = 0; i < (int)bones.size(); ++i) boneMap[bones[i].name] = i;

    // 頂点
    for (unsigned v = 0; v < mesh->mNumVertices; ++v) {
        ModelVertexNew mv{};
        mv.position[0] = mesh->mVertices[v].x;
        mv.position[1] = mesh->mVertices[v].y;
        mv.position[2] = mesh->mVertices[v].z;
        if (mesh->HasNormals()) {
            mv.normal[0] = mesh->mNormals[v].x;
            mv.normal[1] = mesh->mNormals[v].y;
            mv.normal[2] = mesh->mNormals[v].z;
        }
        else {
            mv.normal[0] = 0; mv.normal[1] = 1; mv.normal[2] = 0;
        }
        if (mesh->HasTextureCoords(0)) {
            mv.uv[0] = mesh->mTextureCoords[0][v].x;
            mv.uv[1] = mesh->mTextureCoords[0][v].y;
        }
        else {
            mv.uv[0] = mv.uv[1] = 0.0f;
        }
        for (int k = 0; k < 4; ++k) { mv.boneIndices[k] = 0; mv.boneWeights[k] = 0.0f; }
        vertices.push_back(mv);
    }

    // ボーンウェイト
    if (mesh->HasBones()) {
        struct TmpW { std::vector<std::pair<int, float>> w; };
        std::vector<TmpW> tmp(mesh->mNumVertices);
        for (unsigned bi = 0; bi < mesh->mNumBones; ++bi) {
            aiBone* ab = mesh->mBones[bi];
            std::string bname = ab->mName.C_Str();
            int boneIndex = -1;
            auto it = boneMap.find(bname);
            if (it == boneMap.end()) {
                boneIndex = (int)bones.size();
                boneMap[bname] = boneIndex;
                BoneInfo b;
                b.name = bname;
                b.nodeIndex = FindNodeIndex(nodes, bname);
                b.parentIndex = -1;
                aiMatrix4x4& om = ab->mOffsetMatrix;
                // inverse bind pose (Assimpは既に逆行列)
                b.invBind = XMMatrixSet(
                    om.a1, om.b1, om.c1, om.d1,
                    om.a2, om.b2, om.c2, om.d2,
                    om.a3, om.b3, om.c3, om.d3,
                    om.a4, om.b4, om.c4, om.d4
                );
                bones.push_back(b);
            }
            else boneIndex = it->second;

            for (unsigned w = 0; w < ab->mNumWeights; ++w) {
                uint32_t vid = ab->mWeights[w].mVertexId;
                float val = ab->mWeights[w].mWeight;
                if (vid < tmp.size()) tmp[vid].w.push_back({ boneIndex,val });
            }
        }
        // 正規化 + 上位4個
        for (unsigned v = 0; v < mesh->mNumVertices; ++v) {
            auto& arr = tmp[v].w;
            if (arr.empty()) continue;
            std::sort(arr.begin(), arr.end(), [](auto& a, auto& b) {return a.second > b.second; });
            int count = std::min<int>(4, (int)arr.size());
            float total = 0;
            for (int k = 0; k < count; ++k) { total += arr[k].second; }
            if (total > 0) {
                for (int k = 0; k < count; ++k) {
                    vertices[vtxBase + v].boneIndices[k] = arr[k].first;
                    vertices[vtxBase + v].boneWeights[k] = arr[k].second / total;
                }
            }
        }
    }

    // インデックス
    for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
        aiFace face = mesh->mFaces[f];
        if (face.mNumIndices == 3) {
            indices.push_back(vtxBase + face.mIndices[0]);
            indices.push_back(vtxBase + face.mIndices[1]);
            indices.push_back(vtxBase + face.mIndices[2]);
        }
    }

    sm.indexCount = (uint32_t)indices.size() - sm.indexOffset;
    submeshes.push_back(sm);
}

AnimeTransform ModelManagerRebuild::MakeAnimeTransform(aiVector3D t, aiQuaternion q, aiVector3D s) {
    AnimeTransform at;
    at.translation = { t.x,t.y,t.z };
    at.rotation = { q.x,q.y,q.z,q.w };
    at.scale = { s.x,s.y,s.z };
    return at;
}

void ModelManagerRebuild::ProcessAnimations(const aiScene* scene,
    std::vector<AnimationClip>& clips,
    const std::vector<NodeInfo>& nodes)
{
    if (!scene->HasAnimations()) return;
    for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
        aiAnimation* anim = scene->mAnimations[a];
        AnimationClip clip;
        clip.name = anim->mName.length ? anim->mName.C_Str() : ("Anim_" + std::to_string(a));
        clip.ticksPerSecond = anim->mTicksPerSecond != 0.0 ? (float)anim->mTicksPerSecond : 25.0f;
        clip.durationSeconds = (float)anim->mDuration / clip.ticksPerSecond;
        clip.nodesCopy = nodes;
        for (unsigned ch = 0; ch < anim->mNumChannels; ++ch) {
            aiNodeAnim* na = anim->mChannels[ch];
            AnimationChannel channel;
            channel.nodeIndex = FindNodeIndex(nodes, na->mNodeName.C_Str());
            if (channel.nodeIndex < 0) {
                // ノード見つからない場合はスキップ
            }
            // 各キーを秒換算で挿入
            for (unsigned k = 0; k < na->mNumPositionKeys; ++k) {
                float t = (float)na->mPositionKeys[k].mTime / clip.ticksPerSecond;
                aiVector3D pos = na->mPositionKeys[k].mValue;
                // rotation/scale は後で近い時間のものがあれば統合
                AnimeTransform at;
                at.translation = { pos.x,pos.y,pos.z };
                at.rotation = { 0,0,0,1 };
                at.scale = { 1,1,1 };
                channel.timeline[t] = at;
            }
            for (unsigned k = 0; k < na->mNumRotationKeys; ++k) {
                float t = (float)na->mRotationKeys[k].mTime / clip.ticksPerSecond;
                auto it = channel.timeline.find(t);
                aiQuaternion rot = na->mRotationKeys[k].mValue;
                if (it == channel.timeline.end()) {
                    AnimeTransform at;
                    at.translation = { 0,0,0 };
                    at.rotation = { rot.x,rot.y,rot.z,rot.w };
                    at.scale = { 1,1,1 };
                    channel.timeline[t] = at;
                }
                else {
                    it->second.rotation = { rot.x,rot.y,rot.z,rot.w };
                }
            }
            for (unsigned k = 0; k < na->mNumScalingKeys; ++k) {
                float t = (float)na->mScalingKeys[k].mTime / clip.ticksPerSecond;
                auto it = channel.timeline.find(t);
                aiVector3D sc = na->mScalingKeys[k].mValue;
                if (it == channel.timeline.end()) {
                    AnimeTransform at;
                    at.translation = { 0,0,0 };
                    at.rotation = { 0,0,0,1 };
                    at.scale = { sc.x,sc.y,sc.z };
                    channel.timeline[t] = at;
                }
                else {
                    it->second.scale = { sc.x,sc.y,sc.z };
                }
            }
            clip.channels.push_back(channel);
        }
        clips.push_back(clip);
    }
}

void ModelManagerRebuild::FinalizeBoneParents(std::vector<BoneInfo>& bones) {
    for (auto& b : bones) {
        b.parentIndex = -1;
        // 親は nodeIndex の親ノードとボーン名一致で再探索
        // （必要ならノードの親を辿って firstに一致するBoneを探す）
        // ここでは簡略: 親ノード名 == あるボーン名 なら parentIndex 設定
        // 実運用ではノード配列受け取って処理するが簡略化
    }
}