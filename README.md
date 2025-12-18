# PIXEON2 Engine

## 概要

PIXEON2エンジンはDirectX11で製作している簡易ゲームエンジンです。他エンジンと比べた際、軽量で学習コストが低く、C++による直接的なプログラミングが可能という特徴があります。DLLとして提供されるため、既存のC++プロジェクトに容易に組み込むことができます。

## 特徴

### コアシステム
- **DirectX11ベース**: 高性能なグラフィックスレンダリング
- **コンポーネントベースアーキテクチャ**: 柔軟なゲームオブジェクト設計
- **シーン管理システム**: 複数シーンの管理と切り替え
- **C++ API**: 完全なC++インターフェース

### レンダリング機能
- **2Dレンダリング**: スプライト、アニメーション対応
- **3Dレンダリング**: モデル、ライティング、カメラシステム
- **ブレンドモード**: アルファブレンド、加算合成、減算合成、スクリーンブレンド
- **ポストエフェクト**: カスタマイズ可能なポストプロセシング

### アニメーション
- **2Dアニメーションシステム**: フレームベースアニメーション
- **3Dアニメーション**: ボーンアニメーション、スキンメッシュ
- **タイムラインエディタ**: アニメーションの編集機能
- **イージング機能**: スムーズなアニメーション遷移

### 物理演算
- **Bullet Physics統合**: リアルな物理シミュレーション
- **リジッドボディコンポーネント**: 物体の物理演算
- **コリジョン検出**: ボックスコリジョンなど複数の形状対応
- **コリジョンマネージャー**: 効率的な衝突判定管理

### オーディオシステム
- **サウンドマネージャー**: BGM・SE管理
- **XAudio2統合**: 高品質なオーディオ再生
- **複数フォーマット対応**: WAV、その他の音声フォーマット

### アセット管理
- **テクスチャマネージャー**: 画像リソース管理
- **モデルマネージャー**: 3Dモデルの読み込みと管理
- **シェーダーマネージャー**: カスタムシェーダー管理
- **Assimp統合**: 多様な3Dフォーマットのインポート

### 開発支援ツール
- **ImGUI統合**: デバッグとエディタUI
- **ゲームビュー**: リアルタイムプレビュー
- **インスペクター**: オブジェクトとコンポーネントの編集
- **アニメーションデバッグ**: アニメーション確認ツール

## 主要コンポーネント

### Transform
- 位置（Position）
- 回転（Rotation）
- スケール（Scale）

### CameraComponent
- カメラの位置と向き制御
- 視野角（FOV）設定
- 注視点設定

### ModelRenderComponent
- 3Dモデルの描画
- マテリアル設定
- シェーダー適用

### ImageRender
- 2D画像の描画
- スプライト表示

### Animator2DComponent
- 2Dアニメーションの再生
- フレーム制御
- アニメーション遷移

### AnimationComponent
- 3Dモデルのアニメーション再生
- ボーンアニメーション制御

### RigidBody
- 物理演算の適用
- 質量、摩擦、反発係数の設定

### BoxCollision
- ボックス型の当たり判定
- コリジョンイベント

### LightComponent
- ライティング設定
- ライトの種類（ディレクショナル、ポイント、スポット）

### ScripComponent
- カスタムスクリプトのアタッチ
- ゲームロジックの実装

## API使用例

### シーン管理

```cpp
// 現在のシーンを取得
SceneHandle currentScene;
GetCurrentScene(&currentScene);

// シーン切り替え
ChangeScene("MainScene");
```

### ゲームオブジェクト操作

```cpp
// ゲームオブジェクトを取得
GameObjectHandle player;
GetGameObject(currentScene, "Player", &player);

// トランスフォームを取得
TransformData transform;
GetGameObjectTransform(player, &transform);

// 位置を変更
transform.position.x = 10.0f;
transform.position.y = 5.0f;
SetGameObjectTransform(player, &transform);
```

### 入力処理

```cpp
// キーボード入力
bool isPressed;
IsKeyPressed('W', &isPressed);
if (isPressed) {
    // 前進処理
}

// マウス移動
int mouseX, mouseY;
GetMouseMove(&mouseX, &mouseY);
```

### カメラ制御

```cpp
// カメラコンポーネントを取得
ComponentHandle camera;
GetComponent(cameraObject, "CameraComponent", &camera);

// カメラのトランスフォームを設定
CameraTransform camTransform;
camTransform.position = {0.0f, 5.0f, -10.0f};
camTransform.rotation = {0.0f, 0.0f, 0.0f};
CameraComponent_SetTransform(camera, &camTransform);

// 視野角を設定
CameraComponent_SetFov(camera, 60.0f);
```

## システム要件

### 開発環境
- Visual Studio 2017以降
- Windows 10以降
- DirectX 11対応GPU

### 必要なライブラリ
- DirectX 11
- XAudio2
- Assimp
- Bullet Physics
- ImGUI
- nlohmann/json

## ファイル構成

```
Pixeon_Engine/
├── Pixeon_Engine.dll          # エンジン本体
├── API.h                       # C++ APIヘッダー
├── System.h                    # システム定義
├── Component.h                 # コンポーネント基底クラス
├── Object.h                    # ゲームオブジェクト
├── Scene.h                     # シーン管理
├── コーディング規定.txt        # コーディング規約
├── 命名規則.txt                # 命名規則
└── Pixeon2_Engine_Overview.pdf # 詳細仕様書（PDF版）
```

## コーディング規約

### 変数命名規則
- グローバル変数: `g_` プレフィックス（例：`g_Position`）
- メンバー変数: `m_` プレフィックス（例：`m_health`）
- int型: `i_` プレフィックス（例：`i_count`）
- float型: `f_` プレフィックス（例：`f_speed`）
- bool型: `b_` プレフィックス（例：`b_isActive`）

### 関数命名規則
- 関数名: 小文字から始まるキャメルケース（例：`calculateScore()`）
- 関数は40行以内を推奨

### クラス命名規則
- クラス名: 大文字から始まるキャメルケース（例：`GameManager`）
- インターフェース: `I` プレフィックス（例：`ISerializable`）
- コンポーネント: `Component` サフィックス（例：`CameraComponent`）

## ライセンス

Copyright (c) AC30W

## バージョン情報

- 現在のバージョン: 1.0.0
- エンジン名: PIXEON2 Engine
- ベース: DirectX 11

## サポート

詳細な技術仕様については、`Pixeon2_Engine_Overview.pdf` をご参照ください。

## 今後の予定

- 各コンポーネントのGUIベースの設定機能の実装
- リファクタリングとコーディング規約の全面適用
- 新規コンポーネントのAPI対応拡充
- マルチスレッド対応による描画パフォーマンスの最適化
