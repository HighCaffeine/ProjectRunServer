#pragma once

#include <string>
#include <unordered_map>
#include <vector>
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

    // Gimmick의 복구 시간 검사
    float gimmickRecoverTime;

    //properties
    int hp = 1;
    int weight = 1;
    bool isBombOnly = false;
    int activationType = 0;   // 0 = 상시(기존), 1 = 트리거(밟음)
    float waitTime = 0.0f;    // 1초 등 대기 시간
    float damage = 0.0f;
    float moveSpeed = 0.0f;
    int spawnGimmickKey = 0;
    int monsterType = 0;
    int assignMonsterID = 0;

    //플랫폼 돌아가기위해 추가
    bool isReturning = false;
    float returnDelayTimer = 0.0f;


    bool isMoveTriggered = false;
    float moveDelayTimer = 0.0f;

    bool isInteracting = false;             // 누군가 밀기 시작했는지 여부
    float interactWindowTimer = 0.0f;       // 추가 입력을 기다리는 시간 (예: 0.2초)
    std::vector<uint64_t> interactorUUIDs;  // 0.2초 안에 같이 민 유저들의 ID 목록
    float totalDirX = 0.0f;                 // 합산된 방향 X
    float totalDirZ = 0.0f;                 // 합산된 방향 Z
    float baseForce = 0.0f;                 // 기본으로 가해진 힘
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
    if (typeStr == "Checkpoint") return (UINT8)eGimmickKey::Checkpoint;
    if (typeStr == "BreakableObj") return (UINT8)eGimmickKey::BreakableObj;
    if (typeStr == "Bomb") return (UINT8)eGimmickKey::Bomb;
    if (typeStr == "MonsterSpawnArea") return (UINT8)eGimmickKey::MonsterSpawnArea;

    return (UINT8)eGimmickKey::Gimmick_NONE;
}