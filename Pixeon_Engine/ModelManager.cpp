#define NOMINMAX
#include "AnimationDebug.h"
#include "ModelManager.h"
#include "AssetManager.h"
#include "System.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include "assimp/postprocess.h"
#include <Windows.h>
#include "IMGUI/imgui.h"
#include <filesystem>
#include <unordered_set>
#include <algorithm>
#include "BoneNodeMapping.h"
#include <functional>
#include "MatrixUtil.h"

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
	m_embeddedTextures.clear();
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
		return nullptr;
	}

	auto shared = std::make_shared<ModelSharedResource>();
	shared->source = logicalName;

	// 埋め込みテクスチャの処理
	ProcessEmbeddedTextures(scene, shared, logicalName);

	std::vector<ModelVertex> vertices;
	std::vector<uint32_t> indices;
	ProcessNode(scene->mRootNode, scene, vertices, indices, shared);

	auto device = DirectX11::GetInstance()->GetDevice();
	if (!device) {
		return nullptr;
	}
	{ // VB
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = (UINT)(vertices.size() * sizeof(ModelVertex));
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA srd{ vertices.data(),0,0 };
		if (FAILED(device->CreateBuffer(&bd, &srd, shared->vb.GetAddressOf()))) {
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
			return nullptr;
		}
	}
	shared->vertexCount = (uint32_t)vertices.size();
	shared->indexCount = (uint32_t)indices.size();
	shared->gpuBytes = vertices.size() * sizeof(ModelVertex) + indices.size() * sizeof(uint32_t);

	ProcessMaterials(scene, shared);
	ProcessAnimations(scene, shared);
	ProcessBonesFinalizeHierarchy(scene, shared);
	MapBonesToNodes(*shared);
	QuickIntegrityReport(shared.get());
	DumpBoneChannelMapping(shared.get());
	RebindBoneNodeIndices(shared.get());
	QuickIntegrityReport(shared.get());
#ifdef _DEBUG
	int missing = 0;
	for (auto& b : shared->bones) if (b.nodeIndex < 0) ++missing;
	EditrGUI::GetInstance()->WriteLog("[ModelManager] LoadInternal bone missing nodeIndex=" + std::to_string(missing));
#endif

	return shared;
}

void ModelManager::ProcessEmbeddedTextures(const aiScene* scene,
	std::shared_ptr<ModelSharedResource> shared,
	const std::string& modelName)
{
	if (!scene->HasTextures()) return;

	auto device = DirectX11::GetInstance()->GetDevice();
	if (!device) return;

	for (unsigned int i = 0; i < scene->mNumTextures; ++i) {
		aiTexture* tex = scene->mTextures[i];
		std::string texKey = modelName + ":: *" + std::to_string(i);

		// 既に処理済みならスキップ
		if (m_embeddedTextures.find(texKey) != m_embeddedTextures.end()) {
			continue;
		}

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

		if (tex->mHeight == 0) {
			// 圧縮フォーマット (PNG, JPGなど)
			D3D11_SUBRESOURCE_DATA initData{};
			initData.pSysMem = tex->pcData;
			initData.SysMemPitch = tex->mWidth;

			// DirectXTexやWICを使って読み込むのが理想だが、
			// ここでは簡易的にRAWデータとして扱う
			// 実際のプロジェクトではDirectXTex:: CreateTextureFromMemoryなどを使用推奨

#ifdef _DEBUG
			OutputDebugStringA(("[ModelManager] Embedded compressed texture detected:  " + texKey +
				" format=" + std::string(tex->achFormatHint) + "\n").c_str());
#endif
			// TODO: DirectXTexを使った実装
			// 現状はスキップ
			continue;
		}
		else {
			// 非圧縮RGBA
			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = tex->mWidth;
			desc.Height = tex->mHeight;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			std::vector<uint8_t> rgba(tex->mWidth * tex->mHeight * 4);
			for (unsigned int p = 0; p < tex->mWidth * tex->mHeight; ++p) {
				rgba[p * 4 + 0] = tex->pcData[p].r;
				rgba[p * 4 + 1] = tex->pcData[p].g;
				rgba[p * 4 + 2] = tex->pcData[p].b;
				rgba[p * 4 + 3] = tex->pcData[p].a;
			}

			D3D11_SUBRESOURCE_DATA initData{};
			initData.pSysMem = rgba.data();
			initData.SysMemPitch = tex->mWidth * 4;

			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
			if (SUCCEEDED(device->CreateTexture2D(&desc, &initData, texture.GetAddressOf()))) {
				if (SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf()))) {
					m_embeddedTextures[texKey] = srv;
#ifdef _DEBUG
					OutputDebugStringA(("[ModelManager] Embedded texture loaded: " + texKey + "\n").c_str());
#endif
				}
			}
		}
	}
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ModelManager::GetEmbeddedTexture(
	const std::string& modelName, const std::string& texturePath)
{
	// テクスチャパスが "*数字" 形式なら埋め込みテクスチャ
	if (texturePath.empty() || texturePath[0] != '*') {
		return nullptr;
	}

	std::string key = modelName + "::" + texturePath;
	auto it = m_embeddedTextures.find(key);
	if (it != m_embeddedTextures.end()) {
		return it->second;
	}
	return nullptr;
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
	sm.hasVertexColors = mesh->HasVertexColors(0); // 頂点カラーの有無

	if (sm.skinned) {
		shared->hasSkin = true;
	}
	uint32_t vtxOffset = (uint32_t)vertices.size();

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
				newBone.offset = AssimpToXM_RowMajor(ab->mOffsetMatrix);
				newBone.invOffset = DirectX::XMMatrixInverse(nullptr, newBone.offset);
				newBone.parentIndex = -1;
				newBone.nodeIndex = -1;
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

		// 頂点カラーの取得
		if (mesh->HasVertexColors(0)) {
			mv.color[0] = mesh->mColors[0][v].r;
			mv.color[1] = mesh->mColors[0][v].g;
			mv.color[2] = mesh->mColors[0][v].b;
			mv.color[3] = mesh->mColors[0][v].a;
		}
		else {
			mv.color[0] = mv.color[1] = mv.color[2] = mv.color[3] = 1.0f;
		}

		for (int k = 0; k < 4; ++k) {
			mv.boneIndices[k] = 0;
			mv.boneWeights[k] = 0.0f;
		}

		if (!tmp[v].w.empty()) {
			auto& arr = tmp[v].w;
			std::sort(arr.begin(), arr.end(),
				[](auto& a, auto& b) {return a.second > b.second; });
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

	for (uint32_t f = 0; f < mesh->mNumFaces; ++f) {
		aiFace face = mesh->mFaces[f];
		for (uint32_t j = 0; j < face.mNumIndices; ++j)
			indices.push_back(vtxOffset + face.mIndices[j]);
	}

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

		// ディフューズカラー
		aiColor4D col(1.0f, 1.0f, 1.0f, 1.0f);
		if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, col)) {
			ms.baseColor = { col.r, col.g, col.b, col.a };
		}

		// アンビエントカラー (フォールバック)
		aiColor4D ambient;
		if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_AMBIENT, ambient)) {
			// ディフューズが白の場合はアンビエントを使用
			if (ms.baseColor.x == 1.0f && ms.baseColor.y == 1.0f &&
				ms.baseColor.z == 1.0f && ms.baseColor.w == 1.0f) {
				if (ambient.r != 1.0f || ambient.g != 1.0f || ambient.b != 1.0f) {
					ms.baseColor = { ambient.r, ambient.g, ambient.b, ambient.a };
				}
			}
		}

		// テクスチャ
		aiString texPath;
		if (AI_SUCCESS == mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath)) {
			std::string path = texPath.C_Str();

			// 埋め込みテクスチャかチェック
			if (!path.empty() && path[0] == '*') {
				ms.baseColorTex = path; // "*0", "*1" などをそのまま保存
				ms.isEmbedded = true;
			}
			else {
				ms.baseColorTex = ResolveTexturePath(shared->source, path);
				ms.isEmbedded = false;
			}
		}

		shared->materials.push_back(ms);

#ifdef _DEBUG
		std::string log = "[Material " + std::to_string(i) + "] Color=(" +
			std::to_string(ms.baseColor.x) + "," +
			std::to_string(ms.baseColor.y) + "," +
			std::to_string(ms.baseColor.z) + "," +
			std::to_string(ms.baseColor.w) + ") Tex=" +
			ms.baseColorTex + (ms.isEmbedded ? " (embedded)" : "") + "\n";
		OutputDebugStringA(log.c_str());
#endif
	}
}

std::string ModelManager::ResolveTexturePath(const std::string& modelLogical, const std::string& rawPath) {
	if (rawPath.empty()) return {};
	if (rawPath[0] == '*') {
		// 埋め込みテクスチャはそのまま返す
		return rawPath;
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
	return {};
}

void ModelManager::ProcessBonesFinalizeHierarchy(const aiScene* scene,
	std::shared_ptr<ModelSharedResource> shared)
{
	if (shared->bones.empty()) return;

	std::unordered_map<std::string, std::string> parentName;
	std::function<void(aiNode*, aiNode*)> walk = [&](aiNode* n, aiNode* p) {
		if (!n) return;
		parentName[n->mName.C_Str()] = p ? p->mName.C_Str() : "";
		for (uint32_t c = 0; c < n->mNumChildren; ++c)
			walk(n->mChildren[c], n);
		};
	walk(scene->mRootNode, nullptr);

	std::unordered_map<std::string, int> boneIndexMap;
	for (int i = 0; i < (int)shared->bones.size(); ++i)
		boneIndexMap[shared->bones[i].name] = i;

	for (auto& b : shared->bones) {
		auto pit = parentName.find(b.name);
		if (pit == parentName.end() || pit->second.empty()) {
			b.parentIndex = -1;
		}
		else {
			auto bit = boneIndexMap.find(pit->second);
			b.parentIndex = (bit != boneIndexMap.end()) ? bit->second : -1;
		}
		if (shared->clips.empty()) {
			b.nodeIndex = -1;
		}
	}
#ifdef _DEBUG
	EditrGUI::GetInstance()->WriteLog("[ModelManager] ProcessBonesFinalizeHierarchy done");
#endif
}

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
			for (uint32_t k = 0; k < na->mNumPositionKeys; ++k) {
				float t = (float)(na->mPositionKeys[k].mTime / clip.tps);
				ac.positionKeys.push_back({ t,{
					na->mPositionKeys[k].mValue.x,
					na->mPositionKeys[k].mValue.y,
					na->mPositionKeys[k].mValue.z } });
			}
			for (uint32_t k = 0; k < na->mNumRotationKeys; ++k) {
				float t = (float)(na->mRotationKeys[k].mTime / clip.tps);
				ac.rotationKeys.push_back({ t,{
					na->mRotationKeys[k].mValue.x,
					na->mRotationKeys[k].mValue.y,
					na->mRotationKeys[k].mValue.z,
					na->mRotationKeys[k].mValue.w } });
			}
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
	MapBonesToNodes(*shared);
}

void ModelManager::BuildNodeHierarchy(aiNode* node,
	AnimationClip& clip,
	std::map<std::string, int>& nodeNameToIndex,
	int parentIndex) {
	std::string nm = node->mName.C_Str();

	if (nm.find("$AssimpFbx") != std::string::npos)
	{
		for (uint32_t i = 0; i < node->mNumChildren; ++i)
			BuildNodeHierarchy(node->mChildren[i], clip, nodeNameToIndex, parentIndex);
		return;
	}

	AnimationClip::NodeInfo ni;
	ni.name = nm;
	ni.parentIndex = parentIndex;
	ni.localTransform = AssimpToXM_RowMajor(node->mTransformation);

	int current = (int)clip.nodeHierarchy.size();
	nodeNameToIndex[nm] = current;
	clip.nodeHierarchy.push_back(ni);

	for (uint32_t i = 0; i < node->mNumChildren; ++i)
	{
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
	ImGui::Text("GPU Approx Total: %. 2f MB", totalGPU / (1024.0 * 1024.0));
	ImGui::Text("Embedded Textures: %zu", m_embeddedTextures.size());
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
		ImGui::Text("%s | %s | %. 2f KB | lastUse=%llu",
			kv.first.c_str(),
			aliveRes ? "alive" : "dead",
			kv.second.gpuBytes / 1024.0,
			(unsigned long long)kv.second.lastUse);
	}
	ImGui::EndChild();
}