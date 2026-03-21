#pragma once

#include <cmath>

#ifndef UNITY_H

#define FIXED_DELTA_TIME 0.02f


constexpr int INVENTORY_SIZE = 5;   //인벤토리 사이즈

constexpr float BASE_SPEED = 5.0f;
constexpr int PLAYER_SPEED = 5;
constexpr int TRADE_INVENTORY_SIZE = 9;

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