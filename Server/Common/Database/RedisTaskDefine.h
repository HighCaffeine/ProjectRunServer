#pragma once

#include <cstdint>
#include <cstring>

// ============================================================
//  공용 상수
// ============================================================
constexpr int MAX_USER_ID_LEN = 32;
constexpr int MAX_USER_PW_LEN = 32;
constexpr int MAX_MESSAGE_LEN = 256;
constexpr int INVENTORY_SIZE = 5;
constexpr int EMPTYITEM = 0;

// ============================================================
//  에러 코드 (Redis 태스크 결과용)
// ============================================================
enum class ERROR_CODE : uint16_t
{
    NONE = 0,
    LOGIN_USER_INVALID_PW = 1001,
    LOGIN_USER_NOT_FOUND = 1002,
    INVENTORY_FULL = 2001,
};

// ============================================================
//  Redis 태스크 ID
// ============================================================
enum class RedisTaskID : uint16_t
{
    INVALID = 0,

    // 공지
    REQUEST_NOTICE = 1,
    RESPONSE_NOTICE = 2,

    // 로그인
    REQUEST_LOGIN = 10,
    RESPONSE_LOGIN = 11,

    REQUEST_SET_AUTH_TOKEN = 12,

    // 인벤토리
    REQUEST_LOAD_INVENTORY = 91,
    RESPONSE_LOAD_INVENTORY = 92,

    // 상점 (현재 인게임 미사용, 코드는 유지)
    REQUEST_SHOP_UPDATE = 100,
    RESPONSE_SHOP_UPDATE = 101,
    REQUEST_SHOP_BUY = 102,
    RESPONSE_SHOP_BUY = 103,

    // 거래
    RESPONSE_TRADE_EXCHANGE = 200,
};

// ============================================================
//  Redis 태스크 공통 래퍼
// ============================================================
struct RedisTask
{
    RedisTaskID TaskID = RedisTaskID::INVALID;
    int         UserIndex = -1;
    uint32_t    DataSize = 0;
    char* pData = nullptr;

    void Release()
    {
        if (pData)
        {
            delete[] pData;
            pData = nullptr;
        }
        DataSize = 0;
    }
};

// ============================================================
//  요청 / 응답 구조체
// ============================================================

// --- 로그인 ---
struct RedisLoginReq
{
    char UserID[MAX_USER_ID_LEN + 1] = {};
    char UserPW[MAX_USER_PW_LEN + 1] = {};
};

struct RedisLoginRes
{
    uint16_t Result = 0;
    char     UserID[MAX_USER_ID_LEN + 1] = {};
};

struct RedisAuthTokenReq 
{
    int32_t UserIndex;
    char Token[64];
};

// --- 공지 ---
struct RedisNoticeReq
{
    char Message[MAX_MESSAGE_LEN] = {};
};

struct RedisNoticeRes
{
    char UserID[MAX_USER_ID_LEN + 1] = {};
    char Message[MAX_MESSAGE_LEN] = {};
};

// --- 인벤토리 ---
struct RedisInvenReq
{
    int  UserIndex = -1;
    char UserID[MAX_USER_ID_LEN + 1] = {};
};

struct RedisInvenRes
{
    int UserIndex = -1;
    int ItemSlots[INVENTORY_SIZE] = {};
};

// --- 상점 (인게임 미사용, 코드 유지) ---
struct RedisShopRes
{
    int      ItemID = 0;
    uint64_t NextUpdateTime = 0;
};

struct RedisShopBuyReq
{
    char UserID[MAX_USER_ID_LEN + 1] = {};
    int  itemID = 0;
};

struct RedisShopBuyRes
{
    bool isSuccess = false;
};

// ============================================================
//  유틸리티
// ============================================================
inline void CopyUserID(char* dest, const char* src)
{
    strncpy_s(dest, MAX_USER_ID_LEN + 1, src, _TRUNCATE);
}