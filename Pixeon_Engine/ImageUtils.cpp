/*
* ファイル名　ImageUtils
* 説　　　明　テクスチャを画面上の矩形へ描く補助関数
*             以前は 3 つの関数がほぼ同じ処理（頂点バッファ・入力レイアウト・ステート生成）を
*             それぞれ持っていたため、共通部分を QuadDraw にまとめた
*/
#include "ImageUtils.h"
#include "System.h"
#include "ShaderManager.h"
#include <cstring>
#include <string>
#include <unordered_map>

namespace ImageUtils {
	namespace {
		struct V {
			float p[3];
			float uv[2];
		};

		// VS_ImgQuad / VS_PixelatedEffext / VS_ColorGrading 共通の頂点定数
		struct CBVS {
			DirectX::XMMATRIX View;
			DirectX::XMMATRIX Proj;
			DirectX::XMFLOAT4 Color;
			int mode2D;
			float pad[3];
		};

		// シェーダーの組ごとのリソース
		struct QuadResources {
			RHI::InputLayoutRef layout;
			RHI::BufferRef cbVS;
			RHI::BufferRef cbPS;
		};

		struct SharedResources {
			RHI::BufferRef vb;
			std::unordered_map<std::string, QuadResources> perShader;
		};

		SharedResources& ResourcesGet()
		{
			static SharedResources r;
			return r;
		}

		using QuadParams = QuadDesc;

		/*
		* 関数名　QuadResourcesEnsure
		* 引　数　p：描画パラメータ　vs：頂点シェーダー
		* 戻り値　リソース（失敗時 nullptr）
		* 説　明　シェーダーの組ごとに入力レイアウトと定数バッファを初回だけ作る
		*/
		QuadResources* QuadResourcesEnsure(const QuadParams& p, RHI::Shader* vs)
		{
			RHI::Device* dev = GraphicsSystem::GetInstance()->GetDevice();
			SharedResources& shared = ResourcesGet();
			if (!shared.vb) {
				RHI::BufferDesc bd;
				bd.i_Size = sizeof(V) * 6;
				bd.i_BindFlags = RHI::Bind_VertexBuffer;
				bd.e_Usage = RHI::Usage::Dynamic;
				shared.vb = dev->BufferCreate(bd);
			}
			QuadResources& r = shared.perShader[std::string(p.vsName) + "|" + p.psName];
			if (!r.layout) {
				const RHI::InputElement desc[] = {
					{ "POSITION", 0, RHI::Format::R32G32B32_Float, 0, 0 },
					{ "TEXCOORD", 0, RHI::Format::R32G32_Float, 0, 12 },
				};
				r.layout = dev->InputLayoutCreate(desc, 2, vs);
			}
			if (!r.cbVS) {
				RHI::BufferDesc cbd;
				cbd.i_Size = sizeof(CBVS);
				cbd.i_BindFlags = RHI::Bind_ConstantBuffer;
				r.cbVS = dev->BufferCreate(cbd);
			}
			if (p.psConstants && !r.cbPS) {
				RHI::BufferDesc cbd;
				cbd.i_Size = p.psConstantsSize;
				cbd.i_BindFlags = RHI::Bind_ConstantBuffer;
				r.cbPS = dev->BufferCreate(cbd);
			}
			if (!shared.vb || !r.layout || !r.cbVS || (p.psConstants && !r.cbPS)) return nullptr;
			return &r;
		}

		/*
		* 関数名　QuadVerticesMake
		* 引　数　p：描画パラメータ　out：頂点 6 個
		* 戻り値　なし
		* 説　明　ピクセル座標の矩形をバックバッファ基準の NDC に変換する
		*/
		void QuadVerticesMake(const QuadParams& p, V out[6])
		{
			RHI::Texture* bb = GraphicsSystem::GetInstance()->GetBackBuffer();
			const float W = (p.screenWidth > 0.0f) ? p.screenWidth : (float)bb->GetWidth();
			const float H = (p.screenHeight > 0.0f) ? p.screenHeight : (float)bb->GetHeight();
			auto ndc = [&](float px, float py, float u, float v) {
				return V{ { (px / W) * 2.0f - 1.0f, 1.0f - (py / H) * 2.0f, 0.0f }, { u, v } };
			};
			const auto& uv = p.uvRect;
			const V tl = ndc(p.x, p.y, uv.x, uv.y);
			const V tr = ndc(p.x + p.width, p.y, uv.z, uv.y);
			const V br = ndc(p.x + p.width, p.y + p.height, uv.z, uv.w);
			const V bl = ndc(p.x, p.y + p.height, uv.x, uv.w);
			out[0] = tl; out[1] = tr; out[2] = br;
			out[3] = tl; out[4] = br; out[5] = bl;
		}

		/*
		* 関数名　QuadDraw
		* 引　数　p：描画パラメータ
		* 戻り値　なし
		* 説　明　矩形を描画する。ブレンド・深度ステートは呼び出し前の状態に戻す
		*/
		void QuadDraw(const QuadParams& p)
		{
			if (!p.texture) return;
			GraphicsSystem* gs = GraphicsSystem::GetInstance();
			RHI::Device* dev = gs->GetDevice();
			if (!dev) return;
			ShaderManager* sm = ShaderManager::GetInstance();
			RHI::Shader* vs = sm->GetVertexShader(p.vsName);
			RHI::Shader* ps = sm->GetPixelShader(p.psName);
			if (!vs || !ps) return;
			QuadResources* r = QuadResourcesEnsure(p, vs);
			if (!r) return;
			SharedResources& shared = ResourcesGet();

			V verts[6];
			QuadVerticesMake(p, verts);
			dev->BufferUpdate(shared.vb.get(), verts, sizeof(verts));

			CBVS cb{};
			cb.View = DirectX::XMMatrixIdentity();
			cb.Proj = DirectX::XMMatrixIdentity();
			cb.Color = p.color;
			cb.mode2D = 1;
			dev->BufferUpdate(r->cbVS.get(), &cb, sizeof(cb));
			if (p.psConstants) dev->BufferUpdate(r->cbPS.get(), p.psConstants, p.psConstantsSize);

			RHI::BlendState* oldBlend = dev->BlendStateGet();
			RHI::DepthStencilState* oldDepth = dev->DepthStencilStateGet();

			dev->VertexBufferSet(0, shared.vb.get(), sizeof(V), 0);
			dev->InputLayoutSet(r->layout.get());
			dev->TopologySet(RHI::Topology::TriangleList);
			dev->ShaderSet(RHI::ShaderStage::Vertex, vs);
			dev->ShaderSet(RHI::ShaderStage::Pixel, ps);
			dev->ConstantBufferSet(RHI::ShaderStage::Vertex, 0, r->cbVS.get());
			if (p.psConstants) dev->ConstantBufferSet(RHI::ShaderStage::Pixel, p.psConstantsSlot, r->cbPS.get());
			else dev->ConstantBufferSet(RHI::ShaderStage::Pixel, 0, r->cbVS.get());
			dev->TextureSet(RHI::ShaderStage::Pixel, 0, p.texture);
			gs->SetSamplerState(SAMPLER_LINEAR);
			if (p.blendMode >= 0) gs->SetBlendMode((BlendMode)p.blendMode);
			gs->SetDepthMode(DEPTH_OFF);

			dev->Draw(6, 0);

			dev->TextureSet(RHI::ShaderStage::Pixel, 0, nullptr);
			dev->DepthStencilStateSet(oldDepth);
			dev->BlendStateSet(oldBlend);
		}
	}

	void DrawQuad(const QuadDesc& desc)
	{
		QuadDraw(desc);
	}

	void DrawSRV(RHI::Texture* texture,
		float x, float y, float width, float height,
		const DirectX::XMFLOAT4& color,
		const DirectX::XMFLOAT4& uvRect,
		bool premultipliedAlpha,
		float opacity)
	{
		QuadDesc p;
		p.texture = texture;
		p.x = x; p.y = y; p.width = width; p.height = height;
		p.color = color;
		p.color.w *= opacity;
		p.uvRect = uvRect;
		p.blendMode = premultipliedAlpha ? BLEND_PREMULTIPLIED : BLEND_ALPHA;
		QuadDraw(p);
	}

	void DrawSRVPixelated(RHI::Texture* texture,
		float x, float y, float width, float height,
		float pixelSize,
		float intensity,
		const DirectX::XMFLOAT4& color,
		const DirectX::XMFLOAT4& uvRect)
	{
		RHI::Texture* bb = GraphicsSystem::GetInstance()->GetBackBuffer();
		struct CBPS { float pixelSize; float screenWidth; float screenHeight; float intensity; };
		const CBPS cbps{ pixelSize, (float)(bb ? bb->GetWidth() : 1), (float)(bb ? bb->GetHeight() : 1), intensity };
		QuadDesc p;
		p.vsName = "VS_PixelatedEffext";
		p.psName = "PS_PixelatedEffext";
		p.texture = texture;
		p.x = x; p.y = y; p.width = width; p.height = height;
		p.color = color;
		p.uvRect = uvRect;
		p.psConstants = &cbps;
		p.psConstantsSize = sizeof(cbps);
		p.psConstantsSlot = 1;
		p.blendMode = BLEND_PREMULTIPLIED;
		QuadDraw(p);
	}

	void DrawSRVColorGrading(RHI::Texture* texture,
		float x, float y, float width, float height,
		float brightness,
		float contrast,
		float saturation,
		float hueShift,
		float temperature,
		float tint,
		float gamma,
		const DirectX::XMFLOAT4& uvRect)
	{
		struct CBPS {
			float brightness, contrast, saturation, hueShift;
			float temperature, tint, gamma, padding;
		};
		const CBPS cbps{ brightness, contrast, saturation, hueShift, temperature, tint, gamma, 0.0f };
		QuadDesc p;
		p.vsName = "VS_ColorGrading";
		p.psName = "PS_ColorGrading";
		p.texture = texture;
		p.x = x; p.y = y; p.width = width; p.height = height;
		p.uvRect = uvRect;
		p.psConstants = &cbps;
		p.psConstantsSize = sizeof(cbps);
		p.psConstantsSlot = 0;
		p.blendMode = BLEND_PREMULTIPLIED;
		QuadDraw(p);
	}

	void Release()
	{
		SharedResources& r = ResourcesGet();
		r.vb.reset();
		r.perShader.clear();
	}
}
