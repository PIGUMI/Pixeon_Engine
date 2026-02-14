#include "Animator2DFrame.h"
#include "Input.h"
#include "TimelineEditor.h"
#include "Animator2DManager.h"
#include "CameraComponent.h"

Animator2DFrame* Animator2DFrame::instance = nullptr;

Animator2DFrame* Animator2DFrame::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new Animator2DFrame();
	}
	return instance;
}

void Animator2DFrame::DestroyInstance()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}

void Animator2DFrame::Init()
{
	timelineEditor_ = new TimelineEditor();

	// エディタ用のダミーカメラを作成（親なし）
	editorCamera_ = new CameraComponent();
	editorCamera_->Init(nullptr);

	// カメラの初期設定
	editorCamera_->SetProjectionValues(
		DirectX::XMConvertToRadians(45.0f),  // FOV
		1920.0f / 1080.0f,                    // アスペクト比
		0.1f,                                 // Near
		1000.0f                               // Far
	);

	// UI表示に適したカメラ位置
	Transform camTransform;
	camTransform.position = DirectX::XMFLOAT3(0.0f, 0.0f, -10.0f);
	camTransform.rotation = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	camTransform.scale = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
	editorCamera_->SetTransform(camTransform);

	// 注視点を設定
	editorCamera_->SetFixation(DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

	// デバッグ描画は無効化
	editorCamera_->SetDebugDraw(false);
}

void Animator2DFrame::Update()
{
	if (IsKeyPress(VK_CONTROL) && IsKeyTrigger('S'))
	{
		if (animator_)
		{
			animator_->SaveFile();
			Animator2DManager::GetInstance()->RemoveAllAnimator2D(animator_->GetProjectName());
		}
	}
	if (animator_)
	{
		if (IsKeyTrigger(VK_SPACE))
		{
			isPlaying_ = !isPlaying_;
			if (isPlaying_)animator_->SetFirstFlag(true);
		}
		animator_->SetViewMode(ViewMode::UI);

		// エディタカメラを設定
		if (editorCamera_)
		{
			animator_->SetEditorCamera(editorCamera_);
		}

		if (isPlaying_)
		{
			animator_->Update();
			if (animator_->GetEndedFlag())
			{
				isPlaying_ = false;
			}
		}
		else
		{
			animator_->EditorUpdate();
		}
	}
}

void Animator2DFrame::Draw()
{
	if (animator_)
	{
		animator_->SetEditorMode(true);
		animator_->Draw();
	}
}

void Animator2DFrame::UnInit()
{
	if (animator_)
	{
		animator_->SaveFile();
		delete animator_;
		animator_ = nullptr;
	}
	if (timelineEditor_)
	{
		delete timelineEditor_;
		timelineEditor_ = nullptr;
	}
	if (editorCamera_)
	{
		delete editorCamera_;
		editorCamera_ = nullptr;
	}
}