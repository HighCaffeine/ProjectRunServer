#pragma once

#include <string>
#include <unordered_map>
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "unity.h"
#include <cstdio>

// 서버 메모리에 상주할 기믹 데이터 구조체
struct ServerGimmickData
{
    int gimmickID;
    int type; // eGimmickKey 로 캐스팅해서 사용
    Vector3 position;
    float rotationY;

    // 이동, 추락 발판용 
    Vector3 startPos;
    Vector3 endPos;

    std::unordered_map<std::string, float> properties;

    int currentState; // 0: Off, 1: On, 2: Sync
};

inline UINT8 ConvertGimmickTypeToEnum(const std::string& typeStr)
{
    if (typeStr == "BreakableWall") return (UINT8)eGimmickKey::BreakableWall;
    if (typeStr == "Button") return (UINT8)eGimmickKey::Button;
    if (typeStr == "MovableObject") return (UINT8)eGimmickKey::MovableObject;
    if (typeStr == "Bridge") return (UINT8)eGimmickKey::Bridge;
    if (typeStr == "SeeSaw") return (UINT8)eGimmickKey::SeeSaw;
    if (typeStr == "FallingPlatform") return (UINT8)eGimmickKey::FallingPlatform;
    if (typeStr == "MovePlatform") return (UINT8)eGimmickKey::MovePlatform;
    if (typeStr == "Wind") return (UINT8)eGimmickKey::Wind;
    if (typeStr == "NextZone") return (UINT8)eGimmickKey::NextZone;

    return (UINT8)eGimmickKey::Gimmick_NONE;
}