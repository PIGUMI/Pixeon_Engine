#include "GUI.h"
#include "System.h"
#include "MainFrame.h"
#include "File.h"

#include "AssetManager.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "SoundManager.h"

#include "SceneManger.h"
#include "Scene.h"



GUI* GUI::instance = nullptr;

GUI* GUI::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new GUI();
	}
	return instance;
}

void GUI::DestroyInstance()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}

void GUI::Init()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	auto& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	io.IniFilename = nullptr;

	io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/meiryo.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	ImGui_ImplWin32_Init(MainFrame::GetInstance()->GetWindowHandle());
	ImGui_ImplDX11_Init(DirectX11::GetInstance()->GetDevice(), DirectX11::GetInstance()->GetContext());

	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// タブを完全フラット＆ボーダーレスに
	ImVec4 flatTabColor = ImVec4(0.18f, 0.20f, 0.23f, 1.00f); // 基本のタブ背景色
	ImVec4 flatTabActive = ImVec4(0.20f, 0.22f, 0.25f, 1.00f); // アクティブタブ（ほんのり違い）
	ImVec4 flatTabHovered = ImVec4(0.23f, 0.25f, 0.28f, 1.00f); // ホバー（やや明るく）

	colors[ImGuiCol_Tab] = flatTabColor;
	colors[ImGuiCol_TabUnfocused] = flatTabColor;
	colors[ImGuiCol_TabUnfocusedActive] = flatTabColor;
	colors[ImGuiCol_TabActive] = flatTabColor; // アクティブ時も同じ色に
	colors[ImGuiCol_TabHovered] = flatTabColor; // ホバー時も同じ色に
	colors[ImGuiCol_TabActive] = flatTabActive;
	colors[ImGuiCol_TabHovered] = flatTabHovered;

	// タブとタブのボーダーを完全に消す
	colors[ImGuiCol_Border] = flatTabColor;
	colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

	// タブの角丸を無しに
	style.TabRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.WindowRounding = 0.0f;

	style.TabBorderSize = 0.0f;

	// タブの内側余白を抑えめにして高さも下げる
	style.FramePadding = ImVec2(12, 4);
	style.ItemSpacing = ImVec2(6, 2);

	// タブのテキスト色（アクティブのみ白寄り・非アクティブはグレー寄りで差をつける）
	colors[ImGuiCol_Text] = ImVec4(0.88f, 0.90f, 0.94f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.48f, 0.54f, 1.00f);

	bSceneCreateWindow_		= false;
	bShaderListWindow_		= false;
	bAssetsManagerWindow_	= false;
	bTextureManagerWindow_	= false;
	bModelManagerWindow_	= false;
	bSoundManagerWindow_	= false;
	bSettingWindow_			= false;
}

void GUI::BeginDraw()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	MainMenuBar();
	SceneCreateWindow();
	ShaderListWindow();
	AssetsManagerWindow();
	TextureManagerWindow();
	ModelManagerWindow();
	SoundManagerWindow();
}

void GUI::EndDraw()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}


void GUI::MainMenuBar()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags window_flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_MenuBar;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("MainDock", nullptr, window_flags);
	// メニューバー
	if (ImGui::BeginMenuBar()) {
		const char* guiModes[] = { "Editor","Animator2D" };
		int guiModeWidth = 160;

		float curX = ImGui::GetCursorPosX();
		float targetX = curX - (float)guiModeWidth - 10.0f;
		if (targetX > curX) ImGui::SetCursorPosX(targetX);

		int GuiMode = (int)MainFrame::GetInstance()->GetSoftwareMode();

		ImGui::PushItemWidth((float)guiModeWidth);
		if (ImGui::Combo("##GuiModeCombo", &GuiMode, guiModes, IM_ARRAYSIZE(guiModes))) {
			MainFrame::GetInstance()->SetSoftwareMode((SoftWareMode)GuiMode);
		}

		ImGui::PopItemWidth();
		if (ImGui::BeginMenu(ShiftJISToUTF8("ファイル").c_str()))
		{
			if (ImGui::MenuItem(ShiftJISToUTF8("シーン作成").c_str())) bSceneCreateWindow_ = true;
			ImGui::Separator();

			if (ImGui::MenuItem(ShiftJISToUTF8("シェーダーリスト").c_str())) bShaderListWindow_ = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("アセットマネージャー").c_str())) bAssetsManagerWindow_ = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("テクスチャマネージャー").c_str())) bTextureManagerWindow_ = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("モデルマネージャー").c_str())) bModelManagerWindow_ = true;
			if (ImGui::MenuItem(ShiftJISToUTF8("サウンドマネージャー").c_str())) bSoundManagerWindow_ = true;
			ImGui::Separator();

			if (ImGui::MenuItem(ShiftJISToUTF8("環境設定").c_str())) bSettingWindow_ = true;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(ShiftJISToUTF8("編集").c_str()))
		{
			if (ImGui::MenuItem(ShiftJISToUTF8("入力デバック").c_str())) bInputDebugWindow_ = true;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(ShiftJISToUTF8("ツール").c_str()))
		{
			if (ImGui::MenuItem(ShiftJISToUTF8("シェーダーエディタ").c_str())) bShaderEditorWindow_ = true;

			if (ImGui::MenuItem(ShiftJISToUTF8("フォルダ").c_str())) {
				std::string Path = File::GetExePath();
				Path = File::RemoveExeFromPath(Path);
				File::OpenExplorer(Path);
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void GUI::SceneCreateWindow()
{
	if (!bAssetsManagerWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	if (ImGui::Begin(ShiftJISToUTF8("シーン作成##SceneManager_CreateScene").c_str(), &bSceneCreateWindow_, flags)) {
		static char sceneName[128] = "NewScene";
		ImGui::InputText(ShiftJISToUTF8("シーン名").c_str(), sceneName, sizeof(sceneName));
		ImGui::Separator();
		ImGui::Text(ShiftJISToUTF8("説明").c_str());
		ImGui::TextWrapped(ShiftJISToUTF8("新しいシーンを作成します。シーン名を入力して作成ボタンを押してください。").c_str());
		ImGui::Separator();
		// シーン作成ボタン
		if (ImGui::Button(ShiftJISToUTF8("作成").c_str(), ImVec2(120, 0))) {
			bool OK = SceneManger::GetInstance()->CreateAndRegisterScene(sceneName);
			if (OK) {
				// 作成成功
				MessageBoxA(NULL, "シーンの作成に成功しました。", "成功", MB_OK | MB_ICONINFORMATION);
				// 名前の初期化
				strcpy_s(sceneName, "NewScene");
			}
			else {
				MessageBoxA(NULL, "シーンの作成に失敗しました。", "失敗", MB_OK | MB_ICONERROR);
			}
		}
		// シーンチェンジ
		ImGui::Separator();
		ImGui::Text(ShiftJISToUTF8("現在のシーン:").c_str());
		ImGui::SameLine();
		Scene* current = SceneManger::GetInstance()->GetCurrentScene();
		if (current) {
			std::string currentScene = SceneManger::GetInstance()->GetCurrentScene()->GetName();
			ImGui::Text(ShiftJISToUTF8(("現在のシーン: " + currentScene).c_str()).c_str());
		}

		ImGui::Separator();
		static char changeSceneName[128] = "";
		ImGui::InputText(ShiftJISToUTF8("シーン名##ChangeScene").c_str(), changeSceneName, sizeof(changeSceneName));
		if (ImGui::Button(ShiftJISToUTF8("シーン切り替え").c_str(), ImVec2(120, 0)))SceneManger::GetInstance()->ChangeScene(changeSceneName);

		ImGui::Separator();
		// シーン一覧
		ImGui::Text(ShiftJISToUTF8("シーン一覧").c_str());
		auto sceneList = SceneManger::GetInstance()->GetSceneList();
		ImGui::BeginChild("SceneListChild###EditerGUI___", ImVec2(0, 0), true);
		for (const auto& scene : sceneList) { ImGui::Text(ShiftJISToUTF8(scene).c_str()); };
		ImGui::EndChild();
	}
	ImGui::End();
}

void GUI::ShaderListWindow()
{
	if (!bShaderListWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("シェーダーリスト").c_str(), &bShaderListWindow_, flags)) {
		static int shaderListType = 0;
		const char* shaderListTypes[] = { "Vertex Shader", "Pixel Shader" };
		ImGui::Combo(ShiftJISToUTF8("シェーダーリスト").c_str(), &shaderListType, shaderListTypes, IM_ARRAYSIZE(shaderListTypes));
		ImGui::Separator();
		std::vector<std::string> ShaderList;
		ShaderList.clear();
		if (shaderListType == 0)
			ShaderList = ShaderManager::GetInstance()->GetShaderList("VS");
		else
			ShaderList = ShaderManager::GetInstance()->GetShaderList("PS");
		for (const auto& shaderName : ShaderList) {
			ImGui::Text(ShiftJISToUTF8(shaderName).c_str());
		}
		ImGui::End();
	}
}

void GUI::AssetsManagerWindow()
{
	if (!bAssetsManagerWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("アセットマネージャー").c_str(), &bAssetsManagerWindow_, flags)) {
		AssetManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void GUI::TextureManagerWindow()
{
	if (!bTextureManagerWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("テクスチャマネージャー").c_str(), &bTextureManagerWindow_, flags)) {
		TextureManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void GUI::ModelManagerWindow()
{
	if (!bModelManagerWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("モデルマネージャー").c_str(), &bModelManagerWindow_, flags)) {
		ModelManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}

void GUI::SoundManagerWindow()
{
	if (!bSoundManagerWindow_)return;
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
	if (ImGui::Begin(ShiftJISToUTF8("サウンドマネージャー").c_str(), &bSoundManagerWindow_, flags)) {
		SoundManager::Instance()->DrawDebugGUI();
		ImGui::End();
	}
}


std::string GUI::ShiftJISToUTF8(const std::string& str)
{
	if (str.empty()) return {};

	// Shift-JIS → UTF-16
	int lenW = MultiByteToWideChar(932, 0, str.data(), (int)str.size(), nullptr, 0);
	if (lenW <= 0) return {};

	std::wstring wstr(lenW, 0);
	MultiByteToWideChar(932, 0, str.data(), (int)str.size(), &wstr[0], lenW);

	// UTF-16 → UTF-8
	int lenU8 = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), lenW, nullptr, 0, nullptr, nullptr);
	if (lenU8 <= 0) return {};

	std::string u8str(lenU8, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.data(), lenW, &u8str[0], lenU8, nullptr, nullptr);

	return u8str;
}

ID3D11ShaderResourceView* GUI::LoadImg(const std::wstring& filename, ID3D11Device* device)
{
	IWICImagingFactory* factory = nullptr;
	IWICBitmapDecoder* decoder = nullptr;
	IWICBitmapFrameDecode* frame = nullptr;
	IWICFormatConverter* converter = nullptr;
	ID3D11ShaderResourceView* srv = nullptr;

	// COM は Main::Init() で既に初期化されているため、ここでは初期化不要
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromFilename(filename.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
	if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
	if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
	if (SUCCEEDED(hr)) hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

	UINT width = 0, height = 0;
	if (SUCCEEDED(hr)) hr = frame->GetSize(&width, &height);

	std::vector<BYTE> buffer;
	if (SUCCEEDED(hr)) {
		buffer.resize(width * height * 4);
		hr = converter->CopyPixels(nullptr, width * 4, (UINT)buffer.size(), buffer.data());
	}
	if (SUCCEEDED(hr)) {
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA initData = { buffer.data(), width * 4, 0 };
		ID3D11Texture2D* tex = nullptr;
		hr = device->CreateTexture2D(&desc, &initData, &tex);
		if (SUCCEEDED(hr)) hr = device->CreateShaderResourceView(tex, nullptr, &srv);
		if (tex) tex->Release();
	}

	if (converter) converter->Release();
	if (frame) frame->Release();
	if (decoder) decoder->Release();
	if (factory) factory->Release();
	// COM は Main::UnInit() で終了処理されるため、ここでは終了処理不要

	return srv;
}