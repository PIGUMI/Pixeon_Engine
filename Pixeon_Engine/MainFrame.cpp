#include "MainFrame.h"
#include "System.h"
#include "GameRenderTarget.h"
#include "GUI.h"
// �A�Z�b�g�Ǘ��N���X
#include "AssetManager.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "SoundManager.h"
#include "ComponentManager.h"
#include "ScriptManager.h"
#include "ResourceService.h"
#include "Animator2DManager.h"
// ���͏���
#include "Input.h"
// �\�t�g�E�F�A���[�h
#include "EngineFrame.h"
#include "Animator2DFrame.h"

#include <crtdbg.h>

MainFrame* MainFrame::instance_ = nullptr;

MainFrame* MainFrame::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = new MainFrame();
	}
	return instance_;
}

void MainFrame::DeleteInstance() {
	if (instance_ != nullptr) {
		delete instance_;
		instance_ = nullptr;
	}
}

int MainFrame::Init(const EngineConfig& InPut)
{
	targetFrameTime_ = 1000.0f / 70.0f;
	lastUpdateTime_ = timeGetTime();
	m_hWnd_ = InPut.wnd;
	bUpdateDraw = false;;

	SettingManager::GetInstance()->LoadConfig();
	/* COM �̏����� */
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) return -1;

	/* DirectX11 ������ */
	hr = DirectX11::GetInstance()->Init(InPut.wnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) {
		CoUninitialize();
		return -1;
	}

	/* AssetManager ������ */
	// AssetManager �̃��[�g�p�X�ݒ�
	AssetManager::Instance()->SetRoot(SettingManager::GetInstance()->GetAssetsFilePath());
	// AssetManager �̃��[�h���[�h�ݒ�
	AssetManager::Instance()->SetLoadMode(AssetManager::LoadMode::FromSource);
	// AssetManager �̎��������J�n
	AssetManager::Instance()->StartAutoSync(std::chrono::milliseconds(1000), true);

	/* �G���W���p�����_�[�e�N�X�`�������� */
	// �Q�[���p�����_�[�e�N�X�`��������
	m_gameRenderTarget_ = new GameRenderTarget();
	// �Q�[���p�����_�[�e�N�X�`��������
	m_gameRenderTarget_->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	// Z�o�b�t�@�ݒ�
	m_gameRenderTarget_->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());

	/* GUI�̏����� */
	GUI::GetInstance()->Init();
	/* �V�F�[�_�[�}�l�[�W���[�̏����� */
	ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());

	/* �R���|�[�l���g�}�l�[�W���[�̏����� */
	ComponentManager::GetInstance()->Init();

	/* �X�N���v�g�}�l�[�W���[�̏����� */
	ScriptManager::Instance().RegisterAllScripts();

	/* ���͏����� */
	InitInput();

	EngineFrame::GetInstance()->Init();
	Animator2DFrame::GetInstance()->Init();

	return 0;
}

void MainFrame::Update()
{
	// �t���[������
	DWORD currentTime = timeGetTime();
	float deltaTime = static_cast<float>(currentTime - lastUpdateTime_);

	if (deltaTime >= targetFrameTime_) {
		// deltaTime ��b�P�ʂɕϊ�
		deltaTime_ = deltaTime * 0.001f; // ms -> s
		// ���͍X�V
		UpdateInput(GetWindowHandle());
		// �\�t�g�E�F�A���[�h���Ƃ̍X�V����
		switch (softwareMode_)
		{
		case SoftWareMode::ENGINE:
			EngineFrame::GetInstance()->Update();
			break;
		case SoftWareMode::ANIMTOR2D:
			Animator2DFrame::GetInstance()->Update();
			break;
		default:
			break;
		}

		// �X�V���ԋL�^
		lastUpdateTime_ = currentTime;
		bUpdateDraw = true;
	}
}

void MainFrame::Draw()
{
	if (bUpdateDraw) {
		m_gameRenderTarget_->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());

		m_gameRenderTarget_->Begin(DirectX11::GetInstance()->GetContext());

		// �\�t�g�E�F�A���[�h���Ƃ̕`�揈��
		switch (softwareMode_)
		{
		case SoftWareMode::ENGINE:
			EngineFrame::GetInstance()->Draw();
			break;
		case SoftWareMode::ANIMTOR2D:
			Animator2DFrame::GetInstance()->Draw();
			break;
		default:
			break;
		}

		m_gameRenderTarget_->End();

		// GameRenderTarget��SRV�Ƃ��ĎgpFߑO�ɁA�����_�[�^�[�Q�b�g�Ƃ��Ẵo�C���h�������ɉ��
		// �V�F�[�_�[���\�[�X�X���b�g�����N���A���Ă�SRV�Ƃ��Ă̎g�p�����m�ɂ���
		ID3D11DeviceContext* ctx = DirectX11::GetInstance()->GetContext();
		ID3D11ShaderResourceView* nullSRVs[8] = { nullptr };
		ctx->PSSetShaderResources(0, 8, nullSRVs);
		ctx->VSSetShaderResources(0, 8, nullSRVs);

		DirectX11::GetInstance()->BeginDraw();
		GUI::GetInstance()->BeginDraw();
		// �\�t�g�E�F�A���[�h���Ƃ̕`�揈��
		switch (softwareMode_)
		{
		case SoftWareMode::ENGINE:
			EngineFrame::GetInstance()->DrawGUI();
			break;
		case SoftWareMode::ANIMTOR2D:
			Animator2DFrame::GetInstance()->DrawGUI();
			break;
		default:
			break;
		}
		GUI::GetInstance()->EndDraw();
		DirectX11::GetInstance()->EndDraw();

		bUpdateDraw = false;
	}
}

void MainFrame::UnInit() {
	Animator2DFrame::GetInstance()->UnInit();
	EngineFrame::GetInstance()->UnInit();

	EngineFrame::DestroyInstance();
	Animator2DFrame::DestroyInstance();

	UninitInput();
	// AssetManager �̎���������~
	AssetManager::Instance()->StopAutoSync();
	// �ۑ�
	SettingManager::GetInstance()->SaveConfig();
	// �j������
	// �}�l�[�W���[�̔j��
	GUI::DestroyInstance();
	ComponentManager::DestroyInstance();
	SettingManager::DestroyInstance();
	ScriptManager::Release();
	ShaderManager::DestroyInstance();
	Animator2DManager::GetInstance()->ResetAllAnimator2D();
	AssetManager::DeleteInstance();
	ModelManager::DeleteInstance();
	TextureManager::DeleteInstance();
	SoundManager::DeleteInstance();

	ResourceService::DeleteInstance();

	DirectX11::GetInstance()->Uninit();
	DirectX11::DestroyInstance();
	CoUninitialize();
}

ID3D11ShaderResourceView* MainFrame::GetGameRenderTargetSRV()
{
	return m_gameRenderTarget_->GetShaderResourceView();
}