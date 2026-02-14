#include "HitMarker.h"

void Script_HitMarker::BeginPlay() {
    IScript::BeginPlay(); // BeginPlay
}

void Script_HitMarker:: Update(float DeltaTime) {
    IScript::Update(DeltaTime);// Update
	// ヒットマーカーの表示時間を管理
    if (hitMarkerDuration > 0.0f) {
        hitMarkerDuration -= DeltaTime;
	}
    if (hitMarkerDuration <= 0.0f) {
        // ヒットマーカーの表示時間が終了したら、オブジェクトを削除
        if (_parentObject) {
            _parentScene->RemoveObject(_parentObject);
        }
    }
}

void Script_HitMarker::EndPlay() {
    IScript::EndPlay();// EndPlay
}
