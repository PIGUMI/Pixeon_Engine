#include "Animator2DFrame.h"
#include "Input.h"
#include "TimelineEditor.h"

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
}

void Animator2DFrame::Update()
{
	// XVˆ—
	if(IsKeyPress(VK_CONTROL) && IsKeyTrigger('S'))
	{
		if (animator_)
		{
			animator_->SaveFile();
		}
	}
	if (animator_)
	{
		if(IsKeyTrigger(VK_SPACE))
		{
			isPlaying_ = !isPlaying_;
			if (isPlaying_)animator_->SetFirstFlag(true);
		}
		animator_->SetViewMode(Animator2D::ViewMode::UI);
		if(isPlaying_)
		{ 
			animator_->Update();
			if(animator_->GetEndedFlag())
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
}

