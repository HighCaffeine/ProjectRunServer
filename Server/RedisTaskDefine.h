#pragma once

#define WIN32_LEAN_AND_MEAN
#define EMPTYITEM 0
#include <windows.h>
#include "ErrorCode.h"

#include "unity.h"

enum class RedisTaskID : UINT16
{
	INVALID = 0,

	REQUEST_LOGIN = 1001,
	RESPONSE_LOGIN = 1002,
	REQUEST_NOTICE = 1003,
	RESPONSE_NOTICE = 1004,

	//인벤
	REQUEST_LOAD_INVENTORY = 11001,
	RESPONSE_LOAD_INVENTORY = 11002,

	//상점
	REQUEST_SHOP_UPDATE = 12001,
	RESPONSE_SHOP_UPDATE = 12002,

	REQUEST_SHOP_BUY = 12003,
	RESPONSE_SHOP_BUY = 12004,

	//거래
	REQUEST_TRADE_EXCHANGE = 13001,
	RESPONSE_TRADE_EXCHANGE = 13002,
	
};

enum class ItemID : UINT16
{
	COIN = 101,
	SWORD = 102,
	SHIELD = 103,
	POTION = 104,
	CLOTHES = 105
};



struct RedisTask
{
	UINT32 UserIndex = 0;
	RedisTaskID TaskID = RedisTaskID::INVALID;
	UINT16 DataSize = 0;
	char* pData = nullptr;	

	void Release()
	{
		if (pData != nullptr)
		{
			delete[] pData;
		}
	}
};




#pragma pack(push,1)

struct RedisLoginReq
{
	char UserID[MAX_USER_ID_LEN + 1];
	char UserPW[MAX_USER_PW_LEN + 1];
};

struct RedisLoginRes
{
	char UserID[MAX_USER_ID_LEN + 1];
	UINT16 Result = (UINT16)ERROR_CODE::NONE;
};

struct RedisNoticeReq
{
	char UserID[MAX_USER_ID_LEN + 1];
	char Message[MAX_CHAT_MSG_SIZE + 1];
};

struct RedisNoticeRes
{
	char UserID[MAX_USER_ID_LEN + 1];
	char Message[MAX_CHAT_MSG_SIZE + 1];
};



//인벤토리 결과물
struct RedisInvenReq
{
	int UserIndex;
	char UserID[MAX_USER_ID_LEN + 1];
};

struct RedisInvenRes
{
	int UserIndex;
	int ItemSlots[INVENTORY_SIZE];
};

//거래
struct RedisTradeReq
{
	int UserA, UserB;				//유저 인덱스 정보
	char UserAID[MAX_USER_ID_LEN + 1], UserBID[MAX_USER_ID_LEN + 1]; // 유저 ID
	
	//A 거래 데이터
	int ItemsASlot[INVENTORY_SIZE];
	int ItemsAID[INVENTORY_SIZE];
	
	//B 거래 데이터
	int ItemsBSlot[INVENTORY_SIZE];
	int ItemsBID[INVENTORY_SIZE];
};

struct RedisTradeRes
{
	//단순 성공 여부만 판단, 패킷매니저 내부에서 보내주고
	//클라에서는 서로 뭐 보낼지는 받았으니 그걸로 하거나
	//인벤 요청 혹시 모르니 다시 보내주거나 
	int UserIndex;
	bool IsSuccess;
};

//상점
struct RedisShopReq
{
	int AddHour;	//시간 추가, 0이면 바로 초기화
};

struct RedisShopRes
{
	int ItemID;
	INT64 NextUpdateTime;
};

struct RedisShopBuyReq
{
	char UserID[MAX_USER_ID_LEN + 1];
	int itemID;
};

struct RedisShopBuyRes
{
	bool isSuccess;
};

#pragma pack(pop) //위에 설정된 패킹설정이 사라짐