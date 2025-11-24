#ifndef ENGINE_MANAGER_H
#define ENGINE_MANAGER_H

// エンジンの管理クラス
// 全体管理を行う
// シングルトン

#include <Windows.h>
#include <d3d11.h>
#include <string> 
#include <vector>

class GameRenderTarget;

class EngineManager{
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
	static EngineManager* GetInstance();
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

private:
	void EditeUpdate();
	void InGameUpdate();
	void EditeDraw();
	void InGameDraw();

	EngineManager() {};
	~EngineManager() {};
	
private:
	static EngineManager* instance_;

	DWORD lastUpdateTime_;
	DWORD lastDrawTime_;
	float targetFrameTime_;
	float deltaTime_;

	HWND m_hWnd_;
	GameRenderTarget* m_gameRenderTarget_;
	// ゲーム中判定	
	bool m_bInGame_;
	// GUI表示判定
	bool m_bIsShowGUI_;
	bool m_bIsBeginPlayCalled;
};

#endif // !ENGINE_MANAGER_H

