#include "Input.h"
#include"StartUp.h"
#include "System.h"
#include "EngineManager.h"

//--- グローバル変数
BYTE g_keyTable[256];
BYTE g_oldTable[256];
POINT prevMousePos = { 0, 0 };
int mouseMoveX = 0;
int mouseMoveY = 0;
bool bMouseFreeze = true;

float g_fMouseSensitivity = 0.001f;

HRESULT InitInput()
{
	// 一番最初の入力
	GetKeyboardState(g_keyTable);
	return S_OK;
}
void UninitInput()
{
	while (ShowCursor(TRUE) < 0);
}
void UpdateInput(HWND hWnd)
{
	// 古い入力を更新
	memcpy_s(g_oldTable, sizeof(g_oldTable), g_keyTable, sizeof(g_keyTable));
	// 現在の入力を取得
	GetKeyboardState(g_keyTable);


	POINT CursorPos;
	GetCursorPos(&CursorPos);
	if (hWnd)
	{
		ScreenToClient(hWnd, &CursorPos);
	}

	mouseMoveX = CursorPos.x - prevMousePos.x;
	mouseMoveY = CursorPos.y - prevMousePos.y;

	prevMousePos = CursorPos;

	if (IsKeyTrigger(VK_ESCAPE) && IsKeyPress('P'))
	{
		bMouseFreeze = !bMouseFreeze;
	}

	if (!bMouseFreeze) {
		while (ShowCursor(FALSE) >= 0);
		POINT centerScreen = { 1920 / 2, 1080 / 2 };
		ClientToScreen(EngineManager::GetInstance()->GetWindowHandle(), &centerScreen); // ウィンドウ座標をスクリーン座標に変換
		SetCursorPos(centerScreen.x, centerScreen.y);

		// 中央位置を次回の prev にする
		prevMousePos.x = 1920 / 2;
		prevMousePos.y = 1080 / 2;
	}
	else
	{
		while (ShowCursor(TRUE) < 0);
	}

}

bool IsKeyPress(BYTE key)
{
	return g_keyTable[key] & 0x80;
}
bool IsKeyTrigger(BYTE key)
{
	return (g_keyTable[key] ^ g_oldTable[key]) & g_keyTable[key] & 0x80;
}
bool IsKeyRelease(BYTE key)
{
	return (g_keyTable[key] ^ g_oldTable[key]) & g_oldTable[key] & 0x80;
}
bool IsKeyRepeat(BYTE key)
{
	return false;
}

int MouseMoveX()
{
	return mouseMoveX;
}

int MouseMoveY()
{
	return mouseMoveY;
}

void SetMouseFreeze(bool bFreeze)
{
	bMouseFreeze = bFreeze;
}

float GetMouseSensitivity()
{
	if (bMouseFreeze)
	{
		return 0.0f;
	}
	return g_fMouseSensitivity;
}