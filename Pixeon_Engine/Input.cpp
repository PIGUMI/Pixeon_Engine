#include "Input.h"
#include "StartUp.h"
#include "System.h"
#include "MainFrame.h"
#include <cstring>
#include <WinUser.h>

//--- キーボード / マウス状態
BYTE g_keyTable[256];
BYTE g_oldTable[256];
POINT prevMousePos = { 0, 0 };
int mouseMoveX = 0;
int mouseMoveY = 0;
bool bMouseFreeze = true;

float g_fMouseSensitivity = 0.001f;

// ホイール関連：WndProc で積算（正/負別々に保持）、UpdateInput でフレーム確定
static int g_wheelPosAccum = 0;         // 垂直ホイールの正方向累積（delta 単位）
static int g_wheelNegAccum = 0;         // 垂直ホイールの負方向累積（絶対値で保持）
static int g_wheelForwardFrameDelta = 0; // フレーム確定：正方向（>=0）
static int g_wheelBackwardFrameDelta = 0;// フレーム確定：負方向（>=0）

static int g_hwheelPosAccum = 0;        // 水平ホイール 正方向累積
static int g_hwheelNegAccum = 0;        // 水平ホイール 負方向累積
static int g_hwheelForwardFrameDelta = 0;
static int g_hwheelBackwardFrameDelta = 0;

HRESULT InitInput()
{
	// 初期キーボード状態を取得
	GetKeyboardState(g_keyTable);
	return S_OK;
}
void UninitInput()
{
	while (ShowCursor(TRUE) < 0);
}
void UpdateInput(HWND hWnd)
{
	// キー状態更新（前フレームを退避）
	memcpy_s(g_oldTable, sizeof(g_oldTable), g_keyTable, sizeof(g_keyTable));
	GetKeyboardState(g_keyTable);

	// マウス位置（スクリーン→クライアント）
	POINT CursorPos;
	GetCursorPos(&CursorPos);
	if (hWnd)
	{
		ScreenToClient(hWnd, &CursorPos);
	}

	mouseMoveX = CursorPos.x - prevMousePos.x;
	mouseMoveY = CursorPos.y - prevMousePos.y;

	prevMousePos = CursorPos;

	// フレーム確定：WndProc で積算した正/負をそのままフレーム変数へ渡し、累積はクリア
	g_wheelForwardFrameDelta = g_wheelPosAccum;
	g_wheelBackwardFrameDelta = g_wheelNegAccum;
	g_wheelPosAccum = 0;
	g_wheelNegAccum = 0;

	g_hwheelForwardFrameDelta = g_hwheelPosAccum;
	g_hwheelBackwardFrameDelta = g_hwheelNegAccum;
	g_hwheelPosAccum = 0;
	g_hwheelNegAccum = 0;

	if (IsKeyTrigger(VK_ESCAPE) && IsKeyPress('P'))
	{
		bMouseFreeze = !bMouseFreeze;
	}

	if (!bMouseFreeze) {
		while (ShowCursor(FALSE) >= 0);
		POINT centerScreen = { 1920 / 2, 1080 / 2 };
		ClientToScreen(MainFrame::GetInstance()->GetWindowHandle(), &centerScreen);
		SetCursorPos(centerScreen.x, centerScreen.y);

		// 中央に戻したので prevMousePos も中央にする
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

// WndProc から呼ばれる：垂直ホイール
// delta は通常 WHEEL_DELTA(=120) の倍数。正は前（手前）、負は奥（後ろ）
void OnMouseWheel(short delta)
{
	if (delta > 0) {
		g_wheelPosAccum += (int)delta;
	}
	else if (delta < 0) {
		g_wheelNegAccum += (int)(-delta); // 負方向は絶対値で保持
	}
}

// WndProc から呼ばれる：水平ホイール
void OnMouseHWheel(short delta)
{
	if (delta > 0) {
		g_hwheelPosAccum += (int)delta;
	}
	else if (delta < 0) {
		g_hwheelNegAccum += (int)(-delta);
	}
}

// フレーム内の合算（正負分離）を返す（delta 単位、>=0）
int MouseWheelForwardDelta()
{
	return g_wheelForwardFrameDelta;
}
int MouseWheelBackwardDelta()
{
	return g_wheelBackwardFrameDelta;
}
float MouseWheelForward()
{
	const float fWHEEL_DELTA = 120.0f;
	return g_wheelForwardFrameDelta / fWHEEL_DELTA;
}
float MouseWheelBackward()
{
	const float fWHEEL_DELTA = 120.0f;
	return g_wheelBackwardFrameDelta / fWHEEL_DELTA;
}

// ネット（正 - 負）としての既存 API 互換
int MouseWheelDelta()
{
	return g_wheelForwardFrameDelta - g_wheelBackwardFrameDelta;
}
float MouseWheel()
{
	const float fWHEEL_DELTA = 120.0f;
	return (g_wheelForwardFrameDelta - g_wheelBackwardFrameDelta) / fWHEEL_DELTA;
}

// 水平ホイール版
int MouseHWheelForwardDelta()
{
	return g_hwheelForwardFrameDelta;
}
int MouseHWheelBackwardDelta()
{
	return g_hwheelBackwardFrameDelta;
}
float MouseHWheelForward()
{
	const float fWHEEL_DELTA = 120.0f;
	return g_hwheelForwardFrameDelta / fWHEEL_DELTA;
}
float MouseHWheelBackward()
{
	const float fWHEEL_DELTA = 120.0f;
	return g_hwheelBackwardFrameDelta / fWHEEL_DELTA;
}

int MouseHWheelDelta()
{
	return g_hwheelForwardFrameDelta - g_hwheelBackwardFrameDelta;
}
float MouseHWheel()
{
	const float fWHEEL_DELTA = 120.0f;
	return (g_hwheelForwardFrameDelta - g_hwheelBackwardFrameDelta) / fWHEEL_DELTA;
}

float GetMousePositionX()
{
	return 0.0f;
}

float GetMousePositionY()
{
	return 0.0f;
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