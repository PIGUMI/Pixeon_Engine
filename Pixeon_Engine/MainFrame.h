#ifndef MAIN_FRAME_H
#define MAIN_FRAME_H

#define PIXEON_ENGINE_VERSION "1.1.0"
#define PIXEON_ENGINE_INEDITOR true

// ソフトウェア全体の管理を行うクラス
// 全体管理を行う
// シングルトン
/*
* Log
* 2025/11/25 リファクタリング
* 2025/12/01 リファクタリング
* 2025/12/05 EngineManager から MainFrame に改名
*/

#include <Windows.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <chrono>

class GameRenderTarget;
class Object;

class MainFrame {
private:
	using clock = std::chrono::steady_clock;
public:
	struct EngineConfig {
		HWND		wnd;
		int			screenWidth;
		int			screenHeight;
		const char* windowTitle;
		bool		fullscreen;
		float		targetFPS;
		const char* startScene;
	};
public:
	static MainFrame* GetInstance();
	static void DeleteInstance();

	int Init(const EngineConfig& InPut);
	void Update();
	void Draw();
	void UnInit();

	// Window Handle 取得
	HWND GetWindowHandle() const { return m_hWnd_; }
	// EditorModeのみ有効
	ID3D11ShaderResourceView* GetGameRender();

	// Setter / Getter
	bool IsInGame() const { return m_bInGame_; }
	void SetInGame(bool inGame) { m_bInGame_ = inGame; }

	bool IsShowGUI() const { return m_bIsShowGUI_; }
	void SetShowGUI(bool isShow) { m_bIsShowGUI_ = isShow; }

	float GetDeltaTime() { return deltaTime_; }

	bool AddPrefab(Object* prefab);
	std::vector<Object*> GetPrefabs() const { return prefabs_; }
	Object* GetPrefabByName(const std::string& name);
	void RemovePrefab(Object* ptr);
	GameRenderTarget* GetGameRenderTarget() const { return m_gameRenderTarget_; }

private:
	void EditorUpdate();
	void InGameUpdate();
	void EditorDraw();
	void InGameDraw();

	MainFrame();
	~MainFrame() {};

	void SavePrefabs();
	void LoadPrefabs();

private:
	static MainFrame* instance_;

	DWORD lastUpdateTime_;
	bool bUpdateDraw;
	float targetFrameTime_;
	float deltaTime_;

	HWND m_hWnd_;
	GameRenderTarget* m_gameRenderTarget_;
	// ゲーム中判定
	bool m_bInGame_;
	// GUI表示判定
	bool m_bIsShowGUI_;
	bool m_bIsBeginPlayCalled;
	/* Prefab */
	std::vector<Object*> prefabs_;
};

#endif // !MAIN_FRAME_H
