#include "EngineManager.h"
#include "System.h"
#include "GameRenderTarget.h"
#include "EditrGUI.h"
#include "PostEffectBase.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "AssetManager.h"
#include "SceneManger.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "ComponentManager.h"
#include "ScriptManager.h"
#include "ResourceService.h"
#include "Animator2D.h"
#include "_Geometry.h"
#include "Input.h"
#include "Scene.h"
#include <crtdbg.h>

EngineManager* EngineManager::instance_ = nullptr;

EngineManager* EngineManager::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new EngineManager();
	}
	return instance_;
}

void EngineManager::DeleteInstance() {
	if (instance_ != nullptr) {
		delete instance_;
		instance_ = nullptr;
	}
}

int EngineManager::Init(const EngineConfig& InPut)
{
	/* メンバー変数の初期化 */
	m_bInGame_ = false;
	m_bIsShowGUI_ = false;
	targetFrameTime_ = 1000.0f / 70.0f;
	lastUpdateTime_ = timeGetTime();
	m_hWnd_ = InPut.wnd;
	bUpdateDraw = false;;
	/* 設定の読み込み */
	SettingManager::GetInstance()->LoadConfig();

	/* COM の初期化 */
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) return -1;

	/* DirectX11 初期化 */
	hr = DirectX11::GetInstance()->Init(InPut.wnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) {
		CoUninitialize();
		return -1;
	}

	/* AssetManager 初期化 */
	// AssetManager のルートパス設定
	AssetManager::Instance()->SetRoot(SettingManager::GetInstance()->GetAssetsFilePath());
	// AssetManager のロードモード設定
	AssetManager::Instance()->SetLoadMode(AssetManager::LoadMode::FromSource);
	// AssetManager の自動同期開始
	AssetManager::Instance()->StartAutoSync(std::chrono::milliseconds(1000), true);

	/* エンジン用レンダーテクスチャ初期化 */
	// ゲーム用レンダーテクスチャ初期化
	m_gameRenderTarget_ = new GameRenderTarget();
	// ゲーム用レンダーテクスチャ初期化
	m_gameRenderTarget_->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	// Zバッファ設定
	m_gameRenderTarget_->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());

	/* GUIの初期化 */
	EditrGUI::GetInstance()->Init();

	/* シーンマネージャーの初期化 */
	SceneManger::GetInstance()->Init();
	// 設定の読み込み
	SceneManger::GetInstance()->Load();

	/* シェーダーマネージャーの初期化 */
	ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());

	/* コンポーネントマネージャーの初期化 */
	ComponentManager::GetInstance()->Init();

	/* スクリプトマネージャーの初期化 */
	ScriptManager::Instance().RegisterAllScripts();

	LineRenderer::GetInstance()->Initialize();

	/* 入力初期化 */
	InitInput();

	/* Prefabの読み込み */
	LoadPrefabs();

	return 0;
}

void EngineManager::Update()
{
	// フレーム制御
	DWORD currentTime = timeGetTime();
	float deltaTime = static_cast<float>(currentTime - lastUpdateTime_);

	if (deltaTime >= targetFrameTime_) {
		// deltaTime を秒単位に変換
		deltaTime_ = deltaTime * 0.001f; // ms -> s
		// 入力更新
		UpdateInput(GetWindowHandle());
		if (IsKeyPress(VK_SHIFT) && IsKeyTrigger(VK_RETURN))m_bIsShowGUI_ = !m_bIsShowGUI_;
		// エディタモード・ゲームモード更新
		if (m_bInGame_)
			InGameUpdate();
		else
			EditorUpdate();
		// 更新時間記録
		lastUpdateTime_ = currentTime;
		bUpdateDraw = true;
	}
}

void EngineManager::Draw() {
	if (bUpdateDraw) {
		m_gameRenderTarget_->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
		EditorDraw();
		bUpdateDraw = false;
	}
}

void EngineManager::UnInit() {
	// Prefabの保存
	SavePrefabs();
	/* Prefabのデリート処理 */
	for (auto prefab : prefabs_) {
		if (prefab) {
			delete prefab;
			prefab = nullptr;
		}
	}
	UninitInput();
	LineRenderer::GetInstance()->Finalize();
	// AssetManager の自動同期停止
	AssetManager::Instance()->StopAutoSync();
	// 保存
	SceneManger::GetInstance()->Save();
	SettingManager::GetInstance()->SaveConfig();
	// 破棄処理
	EditrGUI::DestroyInstance();
	AssetManager::DeleteInstance();
	ComponentManager::DestroyInstance();
	SceneManger::DestroyInstance();
	SettingManager::DestroyInstance();
	ShaderManager::DestroyInstance();
	TextureManager::DeleteInstance();
	ModelManager::DeleteInstance();
	ResourceService::DeleteInstance();
	ScriptManager::Release();
	DirectX11::GetInstance()->Uninit();
	DirectX11::DestroyInstance();
	CoUninitialize();
}

ID3D11ShaderResourceView* EngineManager::GetGameRender() {
	return m_gameRenderTarget_->GetShaderResourceView();
}

bool EngineManager::AddPrefab(Object* prefab)
{
	try
	{
		Object* Copy = prefab->Clone();
		Copy->SetParentScene(nullptr);
		// 同じ名前のPrefabが存在する場合、名前に番号を付与
		std::string baseName = Copy->GetObjectName();
		int count = 1;
		while (GetPrefabByName(Copy->GetObjectName())) {
			Copy->SetObjectName(baseName + std::to_string(count));
			count++;
		}
		prefabs_.push_back(Copy);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

Object* EngineManager::GetPrefabByName(const std::string& name)
{
	for (auto prefab : prefabs_) {
		if (prefab->GetObjectName() == name) {
			return prefab;
		}
	}
	return nullptr;
}
// 配列からPrefabを削除
void EngineManager::RemovePrefab(Object* ptr)
{
	prefabs_.erase(std::remove(prefabs_.begin(), prefabs_.end(), ptr), prefabs_.end());
	delete ptr;
	ptr = nullptr;
}

void EngineManager::EditorUpdate() {
	ScriptManager::Instance().Update();
	ShaderManager::GetInstance()->UpdateAndCompileShaders();
	EditrGUI::GetInstance()->Update();
	SceneManger::GetInstance()->EditUpdate();
	m_bIsBeginPlayCalled = false;
}

void EngineManager::InGameUpdate() {
	if (!m_bIsBeginPlayCalled) {
		SceneManger::GetInstance()->BeginPlay();
		m_bIsBeginPlayCalled = true;
	}
	SceneManger::GetInstance()->PlayUpdate();
}

void EngineManager::EditorDraw() {
	m_gameRenderTarget_->Begin(DirectX11::GetInstance()->GetContext());

	switch (EditrGUI::GetInstance()->GetGuiMode()) {
	case 0:
		SceneManger::GetInstance()->Draw();
		break;
	case 1:
		auto View = EditrGUI::GetInstance()->GetAnimator2D();
		if (View)
		{
			View->Draw();
		}
		break;
	}

	m_gameRenderTarget_->End();

	// SRVクリアのタイミングは正しい
	ID3D11DeviceContext* ctx = DirectX11::GetInstance()->GetContext();
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	ctx->PSSetShaderResources(0, 1, nullSRV);

	DirectX11::GetInstance()->BeginDraw();
	EditrGUI::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

void EngineManager::InGameDraw() {
	DirectX11::GetInstance()->BeginDraw();
	SceneManger::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

EngineManager::EngineManager()
	:lastUpdateTime_(0), bUpdateDraw(false), targetFrameTime_(16.67f), deltaTime_(0.0f), m_hWnd_(0),
	m_gameRenderTarget_(nullptr), m_bInGame_(false), m_bIsShowGUI_(false), m_bIsBeginPlayCalled(false)
{
	prefabs_.clear();
}

void EngineManager::SavePrefabs()
{
	std::vector<Object*> SaveObjects;
	SaveObjects = prefabs_;
	// 現在時刻の取得
	auto Now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(Now);
	std::tm localtime;
	localtime_s(&localtime, &in_time_t);

	nlohmann::json SceneData;
	SceneData["SceneSettings"]["Name"] = "prefabs";

	// オブジェクトデータの保存
	nlohmann::json ObjectArray = nlohmann::json::array();

	for (const auto& Object : SaveObjects) {
		if (Object) {
			// オブジェクトの基本情報の保存
			nlohmann::json ObjectData;
			ObjectData["Name"] = Object->GetObjectName();
			ObjectData["Transform"]["Position"] = { Object->GetTransform().position.x, Object->GetTransform().position.y, Object->GetTransform().position.z };
			ObjectData["Transform"]["Rotation"] = { Object->GetTransform().rotation.x, Object->GetTransform().rotation.y, Object->GetTransform().rotation.z };
			ObjectData["Transform"]["Scale"] = { Object->GetTransform().scale.x,    Object->GetTransform().scale.y,    Object->GetTransform().scale.z };

			// コンポーネントデータの保存
			nlohmann::json ComponentData = nlohmann::json::array();
			for (const auto& comp : Object->GetComponents()) {
				if (comp) {
					nlohmann::json CompJson;
					CompJson["Type"] = comp->GetComponentType();
					CompJson["Name"] = comp->GetComponentName();
					std::ostringstream oss;
					comp->SaveToFile(oss);
					CompJson["Data"] = oss.str();
					ComponentData.push_back(CompJson);
				}
			}
			ObjectData["Components"] = ComponentData;
			ObjectArray.push_back(ObjectData);
		}
	}
	SceneData["Objects"] = ObjectArray;

	// ファイル名の生成
	std::string File;
	File = SettingManager::GetInstance()->GetSceneFilePath() + "Prefab" + ".meta";
	std::ofstream outFile(File);
	if (outFile.is_open()) {
		outFile << SceneData.dump(4); // インデント幅4で保存
		outFile.close();
	}
}

void EngineManager::LoadPrefabs()
{
	std::string filePath = SettingManager::GetInstance()->GetSceneFilePath() + "/" + "Prefab" + ".meta";
	std::ifstream inFile(filePath);
	if (!inFile.is_open()) {
		return;
	}

	nlohmann::json sceneData;
	inFile >> sceneData;
	inFile.close();

	// Objectsの読み込み
	for (const auto& objData : sceneData["Objects"]) {
		Object* newObj = new Object();
		newObj->SetParentScene(nullptr);
		newObj->SetObjectName(objData["Name"].get<std::string>());
		// Transformの読み込み
		auto pos = objData["Transform"]["Position"];
		auto rot = objData["Transform"]["Rotation"];
		auto scl = objData["Transform"]["Scale"];
		Transform transform;
		transform.position = { pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>() };
		transform.rotation = { rot[0].get<float>(), rot[1].get<float>(), rot[2].get<float>() };
		transform.scale = { scl[0].get<float>(), scl[1].get<float>(), scl[2].get<float>() };
		newObj->SetTransform(transform);
		// コンポーネントの読み込み
		for (const auto& compData : objData["Components"]) {
			auto type = static_cast<ComponentManager::COMPONENT_TYPE>(compData["Type"].get<int>());
			auto name = compData["Name"].get<std::string>();
			auto data = compData["Data"].get<std::string>();
			Component* newComp = ComponentManager::GetInstance()->AddComponent(newObj, type);
			if (newComp) {
				newComp->SetComponentName(name);
				std::istringstream iss(data);
				newComp->LoadFromFile(iss);
			}
			else {
				MessageBox(nullptr, "コンポーネントの追加に失敗しました", "Error", MB_OK);
			}
		}
		prefabs_.push_back(newObj);
	}
}