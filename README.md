# Pixeon Engine

DirectX 11 / DirectX 12 / Vulkan に対応した自作ゲームエンジンです。
エディタ（シーン編集・ホットリロード）と、エディタから書き出すスタンドアロンのゲーム実行ファイルで構成されています。

## 構成

```
Pixeon_Engine.sln          Visual Studio ソリューション（エンジン本体のビルド）
├─ Pixeon_Engine/          エンジン本体 (DLL)
│   └─ RHI/                描画 API の抽象化層
│       ├─ RHI.h / RHITypes.h   共通インターフェース
│       ├─ D3D11/               DirectX 11 バックエンド
│       ├─ D3D12/               DirectX 12 バックエンド
│       ├─ Vulkan/              Vulkan 1.3 バックエンド（SPIR-V 反射を含む）
│       ├─ DXGI/                D3D11 / D3D12 共通（フォーマット変換・DXBC 反射）
│       └─ RHIImGui.cpp         RHI 上の ImGui 描画（D3D12 / Vulkan 共通）
├─ Exe/                    起動用 exe（エディタ / ゲームの両方を兼ねる）
│   └─ SceneRoot/          プロジェクトのデータ（Assets, Scene, Shader, Script, config）
├─ Common/                 エンジン・ツールで共有する OS 非依存コード
│   ├─ PixArchive          アーカイブ形式 PIXPAK v3
│   ├─ VirtualFileSystem   仮想ファイルシステム（ディレクトリ / アーカイブのマウント）
│   ├─ GameBuilder         ゲームのビルドパイプライン
│   ├─ GameManifest        game.json（ゲームの起動設定）
│   ├─ ShaderCompiler      HLSL → SPIR-V 変換（glslangValidator）
│   └─ ShaderBindings.h    HLSL レジスタと各 API のバインド位置の対応表
├─ BuildExe/               PixeonBuild（コマンドラインのゲームビルドツール）
├─ Asset packaging tool/   PixAssetPacker（アーカイブ作成ツール）
├─ Tests/                  テスト（共通ライブラリ・Vulkan 描画）
└─ CMakeLists.txt          OS 非依存部分とテストのビルド（Linux / Windows）
```

## ビルド

### エンジン（Windows / Visual Studio 2022）

1. `Pixeon_Engine.sln` を開き、構成 `Debug|x64` または `Release|x64` でビルドします。
2. **Vulkan を使う場合**は [Vulkan SDK](https://vulkan.lunarg.com/) をインストールしてください。
   環境変数 `VULKAN_SDK` があると自動で Vulkan バックエンドが有効になります（無い場合は DX11 / DX12 のみでビルドされます）。
   Vulkan SDK に含まれる `glslangValidator` はエディタでの HLSL → SPIR-V 変換にも使われます。

### ツール・テスト（CMake）

```bash
cmake -S . -B build -G Ninja
```

```bash
cmake --build build
```

```bash
ctest --test-dir build --output-on-failure
```

Vulkan ローダーが見つかると `PixRHIVulkanTests` も作られます。GPU が無い環境でも Mesa の lavapipe で動作し、
検証レイヤ（`VK_LAYER_KHRONOS_validation`）があれば有効にして、警告が 1 件でも出ると失敗にします。

## 描画バックエンド (RHI)

エンジンの描画コードは D3D11 のイミディエイトコンテキスト方式（ステートを Set して Draw）で書かれています。
RHI はこれと同じ形の API を提供し、D3D12 / Vulkan では内部で次のようにエミュレートします。

| 項目 | D3D12 / Vulkan での実装 |
|---|---|
| パイプライン | Draw 時点のステートからキーを作り PSO をキャッシュ |
| 定数バッファ・Dynamic バッファ | CPU 側に内容を保持し、変更があればフレームごとのアップロードリングへ書き出す（`UpdateSubresource` / `Map(WRITE_DISCARD)` と同じ結果） |
| ディスクリプタ | D3D12: シェーダー可視ヒープのリング / Vulkan: フレームごとのプールから必要時に確保 |
| 状態遷移 | テクスチャの状態（レイアウト）を追跡して自動でバリア |
| 破棄 | フェンス / タイムラインセマフォで GPU の完了を確認してから解放 |
| 座標系 (Vulkan) | ビューポートの高さを負にして D3D と同じ向きにする |
| シェーダー (Vulkan) | HLSL を glslang で SPIR-V に変換。頂点入力・VS→PS の受け渡しは HLSL のセマンティクス名で対応付け |

シェーダーのレジスタ（`b#` / `t#` / `s#`）はステージごとに CB 8・SRV 16・サンプラー 4 を使えます（`Common/ShaderBindings.h`）。

### バックエンドの選び方

- **エディタ**: `SceneRoot/config/config.json` の `"EditorGraphicsBackend"`（`"DX11"` / `"DX12"` / `"Vulkan"`）
- **ゲーム**: 出力フォルダの `game.json` の `Graphics.Backend`
- **起動オプション**: `--backend dx11|dx12|vulkan`（どちらの設定よりも優先）

指定したバックエンドが使えない環境では DirectX 11 で起動し直します。

## アセット管理

すべてのファイルアクセスは仮想ファイルシステム（VFS）を通ります。仮想パスの先頭で種類を分けています。

| 仮想パス | エディタでの実体 | ゲームでの実体 |
|---|---|---|
| `assets/` | `SceneRoot/Assets`（変更を監視してホットリロード） | アーカイブ |
| `scene/` | `SceneRoot/Scene`（監視） | アーカイブ |
| `shader/cso/`, `shader/spv/` | `SceneRoot/Shader/cso`, `spv` | アーカイブ |
| `config/`, `anim2d/` | `SceneRoot/config` など | アーカイブ |

- アセットは必要になった時点で読み込み、最近使ったものを LRU キャッシュ（64MB）に保持します。
- アーカイブ形式 **PIXPAK v3**: 目次（TOC）に CRC32 を持ち、各ファイルの位置・サイズ・ハッシュを記録します。
  出力はファイル名順で決定的です。v1 / v2 のアーカイブも読めます。
- シェーダーソース（`.hlsl`）やスクリプトのソースはゲームのアーカイブには含めません。

## ゲームのビルド

エディタのメニュー **Build** から、またはコマンドラインで書き出せます。

```bash
PixeonBuild --project Exe --runtime Exe/x64/Release --out Build/MyGame --name MyGame --backend dx12
```

主なオプション: `--scene <開始シーン>` `--width N` `--height N` `--fullscreen` `--no-vsync` `--glslang <path>` `--no-clean`

ビルドの手順:

1. 入力の検証（SceneRoot・実行ファイルの有無）
2. シェーダーのコンパイル（DXBC は .cso を使用、SPIR-V を必要に応じて生成）
3. アセット・シーン・シェーダー・設定を 1 つのアーカイブ（PIXPAK）にまとめる
4. 実行ファイル・DLL・スクリプト DLL をコピー
5. `game.json` と `build_log.txt` を出力

出力フォルダの exe は、隣に `game.json` があるとゲームモードで起動します（`--editor` を付けるとエディタで起動）。

### game.json

```json
{
  "Name": "MyGame",
  "Version": "1.0.0",
  "StartScene": "Title",
  "Archive": "data.pak",
  "Graphics": { "Backend": "DX12", "Width": 1280, "Height": 720, "Fullscreen": false, "VSync": true }
}
```

## 既知の制限

- Effekseer（エフェクト）は DX11 / DX12 のみ対応です。Vulkan では描画されません。
- D3D12 / Vulkan バックエンドは Linux 上のクロスコンパイルによる構文チェックと、
  Vulkan の lavapipe（CPU 実装）での描画テストで検証しています。実機 GPU での動作確認は Windows 環境で行ってください。
- ウィンドウサイズを変更しても、レイヤーのレンダーターゲットや G-Buffer のサイズは変わりません（従来からの仕様）。
- ソリューションの Win32 構成は保守していません。x64 構成を使ってください。
