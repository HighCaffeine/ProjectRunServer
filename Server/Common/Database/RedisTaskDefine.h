#pragma once

#include <cstdint>
#include <cstring>

constexpr int MAX_MESSAGE_LEN = 256;
constexpr int EMPTYITEM = 0;

enum class Redis_ERROR_CODE : uint16_t
{
    NONE = 0,
    LOGIN_USER_INVALID_PW = 10001,
    LOGIN_USER_NOT_FOUND = 10002,
    INVENTORY_FULL = 20001,
};

// ============================================================
//  Redis 테스크 ID
// ============================================================
enum class RedisTaskID : uint16_t
{
    INVALID = 0,

    // --- 1. Notice ---
    REQUEST_NOTICE = 1001,
    RESPONSE_NOTICE = 1002,

    // --- 2. Auth & Login ---
    REQUEST_LOGIN = 1010,
    RESPONSE_LOGIN = 1011,         
    REQUEST_SET_AUTH_TOKEN = 1012,
    REQUEST_VERIFY_TOKEN = 1013,
    RESPONSE_VERIFY_TOKEN = 1014,

    // --- 3. Inventory ---
    REQUEST_LOAD_INVENTORY = 1091,
    RESPONSE_LOAD_INVENTORY = 1092,

    // --- 4. Shop & Trade ---
    //안씀
    REQUEST_SHOP_UPDATE = 1100,
    RESPONSE_SHOP_UPDATE = 1101,
    REQUEST_SHOP_BUY = 1102,
    RESPONSE_SHOP_BUY = 1103,
    RESPONSE_TRADE_EXCHANGE = 1200,

    // --- 5. Ranking ---
    REQUEST_SAVE_RANKING = 1300,
    RESPONSE_SAVE_RANKING = 1301,
};

// ============================================================
//  Redis 테스크
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
//  요청 / 응답 구조체 (기능별 정렬)
// ============================================================

#pragma region [1] Auth & Token
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
    int32_t UserIndex = -1;
    char Token[64] = {};
};

struct RedisAuthTokenRes
{
    UINT16 Result;
};

struct RedisVerifyTokenReq
{
    int32_t UserIndex = -1;
    char Token[64] = {};
};

struct RedisVerifyTokenRes
{
    int32_t UserIndex = -1;
    bool IsValid = false;
};
#pragma endregion

#pragma region [2] Notice
struct RedisNoticeReq
{
    char Message[MAX_MESSAGE_LEN] = {};
};

struct RedisNoticeRes
{
    char UserID[MAX_USER_ID_LEN + 1] = {};
    char Message[MAX_MESSAGE_LEN] = {};
};
#pragma endregion

#pragma region [3] Ranking
struct RedisSaveRankingReq
{
    char UserID[MAX_USER_ID_LEN + 1] = {};
    float ClearTime = 0.0f;
    int32_t DeathCount = 0;
};
#pragma endregion

#pragma region [4] Inventory, Shop
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
#pragma endregion