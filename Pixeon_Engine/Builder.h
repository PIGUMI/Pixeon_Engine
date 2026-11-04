/*
* ファイル名　Builder
* 説　　　明　エディタからゲームをビルドするウィンドウ
*             ビルド処理本体は Common/GameBuilder（コマンドラインの BuildExe と共通）
*             ビルドは別スレッドで実行し、エディタを止めない
*/
#pragma once
#include "../Common/GameBuilder.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class Builder
{
public:
	static Builder* GetInstance();
	static void DeleteInstance();

	void Init();
	void WindowDraw(bool* p_open);
	bool BuildStart();
	bool IsBuilding() const { return m_b_Building.load(); }

private:
	Builder() = default;
	~Builder();
	void SettingsMake(BuildSettings& out_settings) const;
	void ReportDraw();

	static Builder* m_cls_Instance;

	GameManifest m_str_Manifest;
	char m_c_Name[128] = "MyGame";
	char m_c_Version[32] = "1.0.0";
	char m_c_OutputDir[260] = "Build/";
	int  m_i_StartScene = 0;

	std::thread m_cls_Thread;
	std::atomic<bool> m_b_Building{ false };
	std::atomic<float> m_f_Progress{ 0.0f };
	std::string m_Step;
	BuildReport m_str_Report;
	bool m_b_HasReport = false;
	mutable std::mutex m_Mutex;
};
