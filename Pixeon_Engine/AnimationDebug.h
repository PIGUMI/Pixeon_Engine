#pragma once
#include <string>
#include <unordered_map>
#include "AssetTypes.h"
#include "AnimationComponent.h"

// 骨・チャンネル対応ダンプ
void DumpBoneChannelMapping(const ModelSharedResource* res);

// 骨 nodeIndex 再マップ（名前ベース）
int RebindBoneNodeIndices(ModelSharedResource* res);

// 1 つのクリップに対してチャンネル nodeIndex を nodeName から再計算
// （戻り値: 修正したチャンネル数）
int RebindChannelNodeIndices(ModelSharedResource* res, AnimationClipRuntime& runtimeClip);

// 問題サマリを簡易ログ
void QuickIntegrityReport(const ModelSharedResource* res);