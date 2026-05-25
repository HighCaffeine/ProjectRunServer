#pragma once

#include <cmath>
#include <basetsd.h>

#ifndef UNITY_H

#define FIXED_DELTA_TIME 0.02f


constexpr int INVENTORY_SIZE = 5;   //인벤토리 사이즈

constexpr float BASE_SPEED = 5.0f;
constexpr int PLAYER_SPEED = 5;
constexpr int TRADE_INVENTORY_SIZE = 9;

enum eGimmickKey : UINT8
{
    Gimmick_NONE = 0,
    BreakableWall = 1,
    Button = 2,
    MovableObject = 3,
    Bridge = 4,
    SeeSaw = 5,
    PresurePlate = 6,
    FallingPlatform = 7,
    MovePlatform = 8,
    Wind = 9,
    NextZone = 10,
    Checkpoint = 11,
    BreakableObj = 12,
    Bomb = 13,
    MonsterSpawnArea = 14,
    Gimmick_Count = 15
};

enum eGimmickState : UINT8
{
    Off_Destroy = 0, // 꺼짐, 부서짐, 닫힘
    On_Activate = 1, // 켜짐, 작동, 열림
    Sync = 2,        // 지속적인 물리/좌표 동기화
    Restore = 3,     // 다시 초기 위치로 돌아가야 하는 상태
    TriggerMove = 4,
    GimmickPush = 5,
};

enum eState : UINT8
{
    Idle = 0,
    Move = 1,
    Push = 2,
    Pull = 3,
    Dash = 4,
    Knockback = 5,
    Teleport = 6,
    State_Count = 7
};

typedef struct Vector2
{
    float x;
    float y;
} Vector2;

typedef struct Vector3
{
    float x;
    float y;
    float z;
} Vector3;

typedef struct Quaternion
{
    float x;
    float y;
    float z;
    float w;
} Quaternion;

float Vector3_Distance(Vector3 a, Vector3 b);
float Vector3_Distance2D(Vector3 a, Vector3 b);
Vector3 Vector3_right();
Vector3 Vector3_forward();
Vector3 Vector3_Multiply(Vector3 a, float b);
Vector3 Vector3_Addition(Vector3 a, Vector3 b);
Vector3 Quaternion_Multiply(Quaternion rotation, Vector3 point);

#define UNITY_H
#endif