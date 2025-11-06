#ifndef __INPUT_H__
#define __INPUT_H__

#include <Windows.h>
#undef max
#undef min

HRESULT InitInput();
void UninitInput();
void UpdateInput(HWND hWnd);

// WndProc から呼ぶ（WM_MOUSEWHEEL / WM_MOUSEHWHEEL）
void OnMouseWheel(short delta);     // 垂直ホイールの delta（GET_WHEEL_DELTA_WPARAM(wParam)）
void OnMouseHWheel(short delta);    // 水平ホイールの delta（WM_MOUSEHWHEEL）

bool IsKeyPress(BYTE key);
bool IsKeyTrigger(BYTE key);
bool IsKeyRelease(BYTE key);
bool IsKeyRepeat(BYTE key);

int MouseMoveX();
int MouseMoveY();

// フレーム内のホイール量（生の delta, 正負合算）
int MouseWheelDelta();
float MouseWheel();

// フレーム内の垂直ホイールを正方向（手前）と負方向（奥）で別取得
// 返り値は >=0（負方向は絶対値で返す）
int MouseWheelForwardDelta();   // 正方向の合計（delta 単位）
int MouseWheelBackwardDelta();  // 負方向の合計（絶対値, delta 単位）
float MouseWheelForward();      // 正方向のノッチ数（WHEEL_DELTAで割った float）
float MouseWheelBackward();     // 負方向のノッチ数（WHEEL_DELTAで割った float）

int MouseHWheelDelta();
float MouseHWheel();

// 水平ホイールの正/負分離
int MouseHWheelForwardDelta();
int MouseHWheelBackwardDelta();
float MouseHWheelForward();
float MouseHWheelBackward();

// マウス位置（未実装のまま）
float GetMousePositionX();
float GetMousePositionY();

void SetMouseFreeze(bool bFreeze);
float GetMouseSensitivity();

#endif // __INPUT_H__