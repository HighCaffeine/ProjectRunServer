#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <cstring>
#include "..\Utility\unity.h"

const int MAX_USER_ID_LEN = 32;
const int MAX_USER_PW_LEN = 32;

inline void CopyUserID(char* dest, const char* src)
{
	strncpy_s(dest, MAX_USER_ID_LEN + 1, src, _TRUNCATE);
}

inline void CopyUserID(char* dest, const std::string& src)
{
	CopyUserID(dest, src.c_str());
}

#pragma pack(push, 1)

struct PacketInfo
{
	UINT32 ClientIndex = 0;
	UINT16 PacketId = 0;
	UINT16 DataSize = 0;
	char* pDataPtr = nullptr;
};

struct RoomInfo
{
	INT32 roomNum;
	INT32 curUser;
	INT32 maxUser;
	bool isPlaying;
	char title[32];
	INT32 hostPing;
	BYTE guestReadyState;

	INT32 hostCharID;
	INT32 guestCharID;
};

// ============================================================
// PACKET ID
// ============================================================
enum class PACKET_ID : UINT16
{
	// --- 1. System & Time Sync (10 ~ 199) ---
	SYS_USER_CONNECT = 11,
	SYS_USER_DISCONNECT = 12,
	SYS_END = 30,

	SYS_TIME_SYNC_REQ = 101,
	SYS_TIME_SYNC_RES = 102,

	DB_END = 199,

	// --- 2. Lobby & Room Management (200 ~ 239) ---
	LOGIN_REQUEST = 201,
	LOGIN_RESPONSE = 202,
	ROOM_LIST_REQ = 203,
	ROOM_LIST_RES = 204,
	ROOM_ENTER_REQUEST = 206,
	ROOM_ENTER_RESPONSE = 207,
	ROOM_NEW_USER_NTF = 208,
	ROOM_USER_INFO_NTF = 209,

	ROOM_CHAR_SELECT_REQ = 210,
	ROOM_CHAR_SELECT_NTF = 211,

	GAME_START_REQUEST = 215,

	MATCH_START_NTF = 220,
	ROOM_FULL_SYNC_NTF = 222,
	GAME_AUTH_REQUEST = 223,
	GAME_AUTH_RESPONSE = 224,

	ROOM_LEAVE_REQUEST = 225,
	ROOM_LEAVE_RESPONSE = 226,
	ROOM_LEAVE_USER_NTF = 227,
	ROOM_HOST_NTF = 228,
	GAME_CLEAR_RANKING_REQ = 229,

	ROOM_CHAT_REQUEST = 231,
	ROOM_CHAT_RESPONSE = 232,
	ROOM_CHAT_NOTIFY = 233,

	// --- 3. In-Game Physics & Action (240 ~ 269) ---
	PLAYER_MOVEMENT = 241,
	UPDATE_PLAYER_MOVEMENT = 242,
	PLAYER_STATUS_NTF = 243,

	PLAYER_ACTION_REQUEST = 251,
	PLAYER_ACTION_NTF = 252,

	PLAYER_GIMMICK_INTERACT_REQUEST = 261,
	PLAYER_GIMMICK_INTERACT_NTF = 262,
	GIMMICK_BULK_RESET_REQ = 263,
	GIMMICK_BULK_RESET_NTF = 264,

	// --- 4. Game Flow & Dungeon State (270 ~ 299) ---
	PLAYER_READY_REQUEST = 271,
	ROOM_READY_STATUS_NTF = 272,
	GAME_START_COUNTDOWN_NTF = 273,
	GAME_READY_CANCEL_NTF = 274,
	GAME_START_NTF = 275,
	SCENE_SYNC_REQ = 276,

	DUNGEON_ESCAPE_REQ = 281,
	DUNGEON_CLEAR_NTF = 282,
	DUNGEON_RETURN_VILLAGE_REQ = 283,
	DUNGEON_RETURN_VILLAGE_NTF = 284,
	PLAYER_DEAD_REQ = 285,
	PLAYER_DEAD_NTF = 286,

	// --- 5. Shop, Inventory, Trade (300 ~ 399) ---
	INVENTORY_INFO = 301,
	SHOP_INFO = 302,
	SHOP_BUY_REQUEST = 303,
	SHOP_BUY_RESPONSE = 304,

	TRADE_REQUEST = 310,
	TRADE_REQUEST_NTF = 311,
	TRADE_RESPONSE = 312,
	TRADE_START_NTF = 313,
	TRADE_ITEM_UPDATE = 314,
	TRADE_ITEM_NTF = 315,
	TRADE_LOCK = 316,
	TRADE_LOCK_NTF = 317,
	TRADE_CONFIRM = 318,
	TRADE_RESULT = 319,
	TRADE_CONFIRM_NTF = 320,

	// --- 6. Monster Sync (400 ~ ) ---
	MONSTER_SPAWN_NTF = 401,
	MONSTER_MOVEMENT = 402,
	MONSTER_STATE_NTF = 403,
	MONSTER_DEAD_REQ = 404,
	MONSTER_DEAD_NTF = 405,
};



#pragma region [0] PACKET_HEADER
struct PACKET_HEADER
{
	const UINT16 PacketLength;
	const UINT16 PacketId;
	const UINT8 Type;
	PACKET_HEADER(UINT16 PacketLength, PACKET_ID PacketId, UINT8 Type = 0)
		: PacketLength{ PacketLength }, PacketId{ (UINT16)PacketId }, Type{ Type } {
	}
};
const UINT32 PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);
#pragma endregion

#pragma region [1] System & Time Sync
struct TIME_SYNC_REQ_PACKET : public PACKET_HEADER
{
	INT64 clientTimestamp;
	INT32 currentPing;
	TIME_SYNC_REQ_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::SYS_TIME_SYNC_REQ) {}
};

struct TIME_SYNC_RES_PACKET : public PACKET_HEADER
{
	INT64 clientTimestamp;
	INT64 serverTimestamp;
	TIME_SYNC_RES_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::SYS_TIME_SYNC_RES) {}
};
#pragma endregion

#pragma region [2] Lobby & Room Management
struct LOGIN_REQUEST_PACKET : public PACKET_HEADER
{
	char userID[MAX_USER_ID_LEN + 1];
	char userPW[MAX_USER_PW_LEN + 1];
	LOGIN_REQUEST_PACKET() : userID{ 0, }, PACKET_HEADER(sizeof(*this), PACKET_ID::LOGIN_REQUEST) {}
};
const size_t LOGIN_REQUEST_PACKET_SIZE = sizeof(LOGIN_REQUEST_PACKET);

struct LOGIN_RESPONSE_PACKET : public PACKET_HEADER
{
	UINT16 Result;
	INT64 userUUID;
	LOGIN_RESPONSE_PACKET() : Result{ 0 }, PACKET_HEADER(sizeof(*this), PACKET_ID::LOGIN_RESPONSE) {}
};

struct ROOM_LIST_RES_PACKET : public PACKET_HEADER
{
	INT32 roomCount;
	RoomInfo rooms[20];
	ROOM_LIST_RES_PACKET() : roomCount(0), PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_LIST_RES) {}
};

struct ROOM_CHAR_SELECT_REQ_PACKET : public PACKET_HEADER
{
	INT32 charID;
	ROOM_CHAR_SELECT_REQ_PACKET() : charID(0), PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_CHAR_SELECT_REQ) {}
};

struct ROOM_CHAR_SELECT_NTF_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	INT32 charID;
	ROOM_CHAR_SELECT_NTF_PACKET() : userUUID(0), charID(0), PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_CHAR_SELECT_NTF) {}
};

struct GAME_START_REQ_PACKET : public PACKET_HEADER
{
	INT32 roomNumber;
	GAME_START_REQ_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::GAME_START_REQUEST) {}
};

struct ROOM_ENTER_REQUEST_PACKET : public PACKET_HEADER
{
	INT32 RoomNumber;
	char title[32];
	ROOM_ENTER_REQUEST_PACKET() : RoomNumber{ 0 }, title{0}, PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_ENTER_REQUEST) {}
};



struct ROOM_ENTER_RESPONSE_PACKET : public PACKET_HEADER
{
	INT16 Result;
	INT32 roomNum;
	ROOM_ENTER_RESPONSE_PACKET() : Result{ 0 }, PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_ENTER_RESPONSE) {}
};

struct ROOM_NEW_USER_NTF_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	char userID[MAX_USER_ID_LEN + 1];
	INT32 characterID;
	ROOM_NEW_USER_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_NEW_USER_NTF) {}
};

struct ROOM_USER_INFO_NTF_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	char userID[MAX_USER_ID_LEN + 1];
	Vector3 position;
	Quaternion rotation;
	INT32 characterID;
	ROOM_USER_INFO_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_USER_INFO_NTF) {}
};

struct ROOM_HOST_NTF_PACKET : public PACKET_HEADER
{
	INT64 hostUUID;
	ROOM_HOST_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_HOST_NTF) {}
};

struct ROOM_LEAVE_REQUEST_PACKET : public PACKET_HEADER
{
	ROOM_LEAVE_REQUEST_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_LEAVE_REQUEST) {}
};

struct ROOM_LEAVE_RESPONSE_PACKET : public PACKET_HEADER
{
	INT16 Result;
	ROOM_LEAVE_RESPONSE_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_LEAVE_RESPONSE) {}
};

struct ROOM_LEAVE_USER_NTF_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	char userID[MAX_USER_ID_LEN + 1];
	ROOM_LEAVE_USER_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_LEAVE_USER_NTF) {}
};

const static int MAX_CHAT_MSG_SIZE = 256;
struct ROOM_CHAT_REQUEST_PACKET : public PACKET_HEADER
{
	char Message[MAX_CHAT_MSG_SIZE + 1] = { 0, };
	ROOM_CHAT_REQUEST_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_CHAT_REQUEST) {}
};

struct ROOM_CHAT_RESPONSE_PACKET : public PACKET_HEADER
{
	INT16 Result;
	ROOM_CHAT_RESPONSE_PACKET() : Result{ 0 }, PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_CHAT_RESPONSE) {}
};

struct ROOM_CHAT_NOTIFY_PACKET : public PACKET_HEADER
{
	char userID[MAX_USER_ID_LEN + 1] = { 0, };
	char Msg[MAX_CHAT_MSG_SIZE + 1] = { 0, };
	ROOM_CHAT_NOTIFY_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_CHAT_NOTIFY) {}
};
#pragma endregion

#pragma region [3] Game Flow & Handover
struct MATCH_START_NTF_PACKET : public PACKET_HEADER
{
	UINT16 gameServerPort; 
	UINT16 gameServerUdpPort;
	char authToken[64];
	MATCH_START_NTF_PACKET() : authToken{ 0 }, gameServerPort{ 0 }, PACKET_HEADER(sizeof(*this), PACKET_ID::MATCH_START_NTF) {}
};

struct GAME_AUTH_REQUEST_PACKET : public PACKET_HEADER
{
	char authToken[64];
	char userName[33];
	INT32 characterID;
	GAME_AUTH_REQUEST_PACKET() : authToken{ 0 }, userName{0}, PACKET_HEADER(sizeof(*this), PACKET_ID::GAME_AUTH_REQUEST) {}
};

struct GAME_AUTH_RESPONSE_PACKET : public PACKET_HEADER
{
	UINT16 Result;
	GAME_AUTH_RESPONSE_PACKET() : Result(0), PACKET_HEADER(sizeof(*this), PACKET_ID::GAME_AUTH_RESPONSE) {}
};

struct PLAYER_READY_REQUEST_PACKET : public PACKET_HEADER
{
	bool isReady;
	PLAYER_READY_REQUEST_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_READY_REQUEST) {}
};

struct ROOM_READY_STATUS_NTF_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	bool isReady;
	ROOM_READY_STATUS_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_READY_STATUS_NTF) {}
};

struct GAME_START_COUNTDOWN_NTF_PACKET : public PACKET_HEADER
{
	int remainSeconds;
	GAME_START_COUNTDOWN_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::GAME_START_COUNTDOWN_NTF) {}
};

struct GAME_START_NTF_PACKET : public PACKET_HEADER
{
	int mapId;
	GAME_START_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::GAME_START_NTF) {}
};

struct GAME_READY_CANCEL_NTF_PACKET : public PACKET_HEADER
{
	char dummy = 0;
	GAME_READY_CANCEL_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::GAME_READY_CANCEL_NTF) {}
};

struct SCENE_SYNC_REQ_PACKET : public PACKET_HEADER
{
	char dummy = 0;
	SCENE_SYNC_REQ_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::SCENE_SYNC_REQ) {}
};

struct DUNGEON_ESCAPE_REQ_PACKET : public PACKET_HEADER
{
	INT32 p1Push;
	INT32 p1Pull;
	INT32 p1Fall;
	INT32 p1Destroy;   
	INT32 p1FallKill;  
	INT32 p2Push;
	INT32 p2Pull;
	INT32 p2Fall;
	INT32 p2Destroy;  
	INT32 p2FallKill; 
	DUNGEON_ESCAPE_REQ_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::DUNGEON_ESCAPE_REQ) {}
};

struct DUNGEON_CLEAR_NTF_PACKET : public PACKET_HEADER
{
	INT32 clearTimeSeconds;
	INT32 p1Push;
	INT32 p1Pull;
	INT32 p1Fall;
	INT32 p1Destroy;   
	INT32 p1FallKill; 
	INT32 p2Push;
	INT32 p2Pull;
	INT32 p2Fall;
	INT32 p2Destroy;  
	INT32 p2FallKill;  
	DUNGEON_CLEAR_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::DUNGEON_CLEAR_NTF) {}
};

struct DUNGEON_RETURN_VILLAGE_REQ_PACKET : public PACKET_HEADER 
{
	BYTE dummy;
	DUNGEON_RETURN_VILLAGE_REQ_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::DUNGEON_RETURN_VILLAGE_REQ) { }
};
struct DUNGEON_RETURN_VILLAGE_NTF_PACKET : public PACKET_HEADER 
{
	BYTE dummy;
	DUNGEON_RETURN_VILLAGE_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::DUNGEON_RETURN_VILLAGE_NTF) { }
};

struct GAME_CLEAR_RANKING_REQ_PACKET : public PACKET_HEADER
{
	float clearTime;
	INT32 deathCount;
	GAME_CLEAR_RANKING_REQ_PACKET() : clearTime(0.0f), deathCount(0), PACKET_HEADER(sizeof(*this), PACKET_ID::GAME_CLEAR_RANKING_REQ) {}
};
#pragma endregion

#pragma region [4] In-Game Physics & Action
struct PLAYER_MOVEMENT_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	UINT32 inputSeq;
	Vector3 currentPos;
	Quaternion currentRot;
	float axisH;
	float axisV;
	PLAYER_MOVEMENT_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_MOVEMENT) {}
};

struct UPDATE_PLAYER_MOVEMENT_PACKET : public PACKET_HEADER
{
	UINT32 lastInputSeq;
	INT64 userUUID;
	Vector3 currentPos;
	Quaternion currentRot;
	float currentSpeed;
	float axisH;
	float axisV;
	bool isMoving;
	UPDATE_PLAYER_MOVEMENT_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::UPDATE_PLAYER_MOVEMENT) {}
};

struct PLAYER_STATUS_NTF_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	UINT8 newState;
	Vector3 targetDir;
	float powerOrTime;
	UINT8 isPull;
	Vector3 casterPos;
	INT64 timestamp;
	INT64 casterUUID;
	PLAYER_STATUS_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_STATUS_NTF) {}
};

struct PLAYER_ACTION_REQUEST_PACKET : public PACKET_HEADER
{
	INT64 targetUUID;
	UINT8 actionType;
	PLAYER_ACTION_REQUEST_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_ACTION_REQUEST) {}
};

struct PLAYER_ACTION_NTF_PACKET : public PACKET_HEADER
{
	INT64 attackerUUID;
	INT64 targetUUID;
	UINT8 actionType;
	PLAYER_ACTION_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_ACTION_NTF) {}
};

struct PLAYER_GIMMICK_INTERACT_REQUEST_PACKET : public PACKET_HEADER
{
	INT64 activeUUID;
	INT32 gimmickID;
	UINT8 gimmickKey;
	UINT8 state;
	Vector3 targetPos;
	float param;
	INT64 timestamp;
	PLAYER_GIMMICK_INTERACT_REQUEST_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_GIMMICK_INTERACT_REQUEST) {}
};

struct PLAYER_GIMMICK_INTERACT_NTF_PACKET : public PACKET_HEADER
{
	INT64 activeUUID;
	INT32 gimmickID;
	UINT8 gimmickKey;
	UINT8 state;
	Vector3 targetPos;
	float param;
	INT64 timestamp;
	PLAYER_GIMMICK_INTERACT_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_GIMMICK_INTERACT_NTF) {}
};

struct GIMMICK_BULK_RESET_PACKET : public PACKET_HEADER
{
	INT32 count;
	INT32 gimmickIDs[20];

	GIMMICK_BULK_RESET_PACKET(PACKET_ID id = PACKET_ID::GIMMICK_BULK_RESET_REQ) : PACKET_HEADER(sizeof(*this), id)
	{
		count = 0;
		memset(gimmickIDs, 0, sizeof(gimmickIDs));
	}
};

struct PLAYER_DEAD_REQ_PACKET : public PACKET_HEADER
{
	Vector3 respawnPos;
	PLAYER_DEAD_REQ_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_DEAD_REQ) {}
};

struct PLAYER_DEAD_NTF_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	Vector3 respawnPos;
	PLAYER_DEAD_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_DEAD_NTF) {}
};
#pragma endregion

#pragma region [5] Monster Sync
struct MONSTER_MOVEMENT_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	int monsterID;
	Vector3 currentPos;
	Quaternion currentRot;
	INT64 timestamp;
	MONSTER_MOVEMENT_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::MONSTER_MOVEMENT) {}
};

struct MONSTER_STATE_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	INT32 monsterID;
	UINT8 newState;
	Vector3 targetDir;
	float param;
	UINT8 isPull;
	Vector3 casterPos;
	INT64 timeStamp;
	MONSTER_STATE_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::MONSTER_STATE_NTF) {}
};

struct MONSTER_DEAD_REQ_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	int monsterID;
	MONSTER_DEAD_REQ_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::MONSTER_DEAD_REQ) {}
};

struct MONSTER_DEAD_NTF_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	int monsterID;
	MONSTER_DEAD_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::MONSTER_DEAD_NTF) {}
};
#pragma endregion

#pragma region [6] Shop, Inventory, Trade
struct INVENTORY_INFO_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	INT32 itemIDs[INVENTORY_SIZE] = { 0, };
	INVENTORY_INFO_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::INVENTORY_INFO) {}
};

struct SHOP_INFO_PACKET : public PACKET_HEADER
{
	INT32 currentItemID;
	INT64 nextUpdateTime;
	SHOP_INFO_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::SHOP_INFO) {}
};

struct SHOP_BUY_REQUEST_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	INT32 itemID;
	SHOP_BUY_REQUEST_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::SHOP_BUY_REQUEST) {}
};

struct SHOP_BUY_RESPONSE_PACKET : public PACKET_HEADER
{
	bool isSuccess;
	SHOP_BUY_RESPONSE_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::SHOP_BUY_RESPONSE) { isSuccess = false; }
};

struct TRADE_REQUEST_PACKET : public PACKET_HEADER
{
	INT64 targetUUID;
	TRADE_REQUEST_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_REQUEST) {}
};

struct TRADE_REQUEST_NTF_PACKET : public PACKET_HEADER
{
	INT64 reqUUID;
	char reqName[MAX_USER_ID_LEN + 1];
	TRADE_REQUEST_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_REQUEST_NTF) {}
};

struct TRADE_RESPONSE_PACKET : public PACKET_HEADER
{
	INT64 tradeUUID;
	bool isAccept;
	TRADE_RESPONSE_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_RESPONSE) {}
};

struct TRADE_START_NTF_PACKET : public PACKET_HEADER
{
	INT64 tradeUUID;
	char reqName[MAX_USER_ID_LEN + 1] = { 0, };
	TRADE_START_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_START_NTF) {}
};

struct TRADE_ITEM_UPDATE_PACKET : public PACKET_HEADER
{
	INT32 tradeSlot;
	INT32 invenSlot;
	INT32 itemID;
	TRADE_ITEM_UPDATE_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_ITEM_UPDATE) {}
};

struct TRADE_ITEM_NTF_PACKET : public PACKET_HEADER
{
	INT32 index;
	INT32 itemID;
	TRADE_ITEM_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_ITEM_NTF) {}
};

struct TRADE_LOCK_PACKET : public PACKET_HEADER
{
	bool isLock;
	TRADE_LOCK_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_LOCK) {}
};

struct TRADE_LOCK_NTF_PACKET : public PACKET_HEADER
{
	bool isLock;
	TRADE_LOCK_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_LOCK_NTF) {}
};

struct TRADE_CONFIRM_PACKET : public PACKET_HEADER
{
	bool isConfirm;
	TRADE_CONFIRM_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_CONFIRM) {}
};

struct TRADE_CONFIRM_NTF_PACKET : public PACKET_HEADER
{
	bool isConfirm;
	INT64 confirmUserUUID;
	TRADE_CONFIRM_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_CONFIRM_NTF) {}
};

struct TRADE_RESULT_PACKET : public PACKET_HEADER
{
	bool isSuccess;
	TRADE_RESULT_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_RESULT) {}
};
#pragma endregion

#pragma pack(pop)