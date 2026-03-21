#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "unity.h"

struct RawPacketData
{
	UINT32 ClientIndex = 0;
	UINT32 DataSize = 0;
	char* pPacketData = nullptr;

	void Set(RawPacketData& vlaue)
	{
		ClientIndex = vlaue.ClientIndex;
		DataSize = vlaue.DataSize;

		pPacketData = new char[vlaue.DataSize];
		CopyMemory(pPacketData, vlaue.pPacketData, vlaue.DataSize);
	}

	void Set(UINT32 clientIndex_, UINT32 dataSize_, char* pData)
	{
		ClientIndex = clientIndex_;
		DataSize = dataSize_;

		pPacketData = new char[dataSize_];
		CopyMemory(pPacketData, pData, dataSize_);
	}

	void Release()
	{
		delete pPacketData;
	}
};


struct PacketInfo
{
	UINT32 ClientIndex = 0;
	UINT16 PacketId = 0;
	UINT16 DataSize = 0;
	char* pDataPtr = nullptr;
};


enum class  PACKET_ID : UINT16
{
	//SYSTEM
	SYS_USER_CONNECT = 11,
	SYS_USER_DISCONNECT = 12,
	SYS_END = 30,

	//DB
	DB_END = 199,

	//Client
	LOGIN_REQUEST = 201,
	LOGIN_RESPONSE = 202,

	// Enter
	ROOM_ENTER_REQUEST = 206,
	ROOM_ENTER_RESPONSE = 207,
	ROOM_NEW_USER_NTF = 208, // 입장하는 유저에게도 전송
	ROOM_USER_INFO_NTF = 209, // Zone에 있던 유저 정보 (입장하는 유저에게만 보냄)

	// Leave
	ROOM_LEAVE_REQUEST = 215,
	ROOM_LEAVE_RESPONSE = 216,
	ROOM_LEAVE_USER_NTF = 217,

	// Chat
	ROOM_CHAT_REQUEST = 221, // SEND_CHAT_MESSAGE
	ROOM_CHAT_RESPONSE = 222,
	ROOM_CHAT_NOTIFY = 223, // RECEIVE_CHAT_MESSAGE

	// Move
	PLAYER_MOVEMENT = 231,
	UPDATE_PLAYER_MOVEMENT = 232,
	PLAYER_STATUS_NTF = 233,

	// Path
	MOVE_PATH_REQUEST = 241,
	MOVE_PATH_RESPONSE = 242,
	MOVE_PATH_NOTIFY = 243,

	// Physics
	PLAYER_ACTION_REQUEST = 251,

	//인벤 / 상점용
	INVENTORY_INFO = 301,       // 접속갱신 시 인벤토리 정보 전송
	SHOP_INFO = 302,            // 상점 정보 - 현재 판매 아이템, 다음 갱신 시간
	SHOP_BUY_REQUEST = 303,		//아이템 구매 요청
	SHOP_BUY_RESPONSE = 304,	//아이템 구매 결과

	//거래용
	TRADE_REQUEST = 310,        // A -> Server: 교환 요청
	TRADE_REQUEST_NTF = 311,    // Server -> B: A가 요청함 
	TRADE_RESPONSE = 312,       // B -> Server: 거래 수락 / 거절
	TRADE_START_NTF = 313,      // Server -> A, 
	// A : 거래 거절 시 거래창 닫기 B: 거래창 열기

	TRADE_ITEM_UPDATE = 314,    // A,B -> Server: 아이템 등록 
	TRADE_ITEM_NTF = 315,       // Server -> A,B: A / B가 아이템 올렸으니 업데이트 
	TRADE_LOCK = 316,           // A,B -> Server: 아이템 확정 
	//(2번째 온 애 거를 기준으로 confirm 패킷 전송)
	TRADE_LOCK_NTF = 317,       // Server -> A,B: A / B의 Lock 상태 받음

	TRADE_CONFIRM = 318,        // A,B -> Server: 최종 교환 버튼
	TRADE_RESULT = 319,         // Server -> A, B: 거래 성공/실패 결과

	TRADE_CONFIRM_NTF = 320,
};

#pragma pack(push,1)
#pragma region PACKET_HEADER
struct PACKET_HEADER
{
	const UINT16 PacketLength;
	const UINT16 PacketId;
	const UINT8 Type; //압축여부 암호화여부 등 속성을 알아내는 값
	PACKET_HEADER(UINT16 PacketLength, PACKET_ID PacketId, UINT8 Type = 0) : PacketLength{ PacketLength }, PacketId{ (UINT16)PacketId }, Type{Type}
	{
	}
};
const UINT32 PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);
#pragma endregion

#pragma region Login Packets
const int MAX_USER_ID_LEN = 32;
const int MAX_USER_PW_LEN = 32;

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

	LOGIN_RESPONSE_PACKET() : Result{ 0 }, PACKET_HEADER(sizeof(*this), PACKET_ID::LOGIN_RESPONSE) {}
};
#pragma endregion

#pragma region Room Enter Packets
//const int MAX_ROOM_TITLE_SIZE = 32;
struct ROOM_ENTER_REQUEST_PACKET : public PACKET_HEADER
{
	INT32 RoomNumber;
	ROOM_ENTER_REQUEST_PACKET() : RoomNumber{ 0 }, PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_ENTER_REQUEST) {}
};

struct ROOM_ENTER_RESPONSE_PACKET : public PACKET_HEADER
{
	INT16 Result;
	//char RivaluserID[MAX_USER_ID_LEN + 1] = { 0, };
	ROOM_ENTER_RESPONSE_PACKET() : Result{ 0 }, PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_ENTER_RESPONSE) {}
};


struct ROOM_NEW_USER_NTF_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	char userID[MAX_USER_ID_LEN + 1];

	ROOM_NEW_USER_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_NEW_USER_NTF) {}
};

struct ROOM_USER_INFO_NTF_PACKET : public PACKET_HEADER
{

	INT64 userUUID;
	char userID[MAX_USER_ID_LEN + 1];
	Vector3 position;
	Quaternion rotation;

	ROOM_USER_INFO_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::ROOM_USER_INFO_NTF) {}
};
#pragma endregion

#pragma region Player Movement Packets
//struct PLAYER_MOVEMENT_PACKET : public PACKET_HEADER
//{
//	INT64 userUUID;
//	float dx;
//	float dy;
//	Quaternion rotation;
//
//	PLAYER_MOVEMENT_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_MOVEMENT) {}
//};


//struct UPDATE_PLAYER_MOVEMENT_PACKET : public PACKET_HEADER
//{
//	INT64 userUUID;
//	Quaternion rotation;
//	Vector3 motion;
//
//	UPDATE_PLAYER_MOVEMENT_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::UPDATE_PLAYER_MOVEMENT) {}
//};
//struct PLAYER_MOVEMENT_PACKET : public PACKET_HEADER
//{
//	INT64 userUUID;
//	UINT32 inputSeq; // 보정용 번호
//	Vector3 targetPos;	
//
//	PLAYER_MOVEMENT_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_MOVEMENT) {}
//};
//
//struct UPDATE_PLAYER_MOVEMENT_PACKET : public PACKET_HEADER
//{
//	UINT32 lastInputSeq; // 처리된 번호
//	INT64 userUUID;
//	Vector3 currentPos;  // 서버가 계산한 현재 실시간 좌표
//	bool isMoving;       // 현재 이동 중인지 여부
//
//	UPDATE_PLAYER_MOVEMENT_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::UPDATE_PLAYER_MOVEMENT) {}
//};
//#pragma endregion

#pragma region Room Leave Packets
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
#pragma endregion

#pragma region Room Chat Packets
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

#pragma region Move Packets

//구버전 이동 패킷
struct MOVE_PATH_REQUEST_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	Vector3 startPos;
	Vector3 endPos;

	MOVE_PATH_REQUEST_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::MOVE_PATH_REQUEST) {}
};
struct MOVE_PATH_RESPONSE_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	Vector3 path[10];
	INT16 pathCount;

	MOVE_PATH_RESPONSE_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::MOVE_PATH_RESPONSE) {}
};

//신버전 이동 패킷
// Client -> Server: WASD 입력 패킷
struct PLAYER_MOVEMENT_PACKET : public PACKET_HEADER 
{
	INT64 userUUID;
	UINT32 inputSeq;    // 클라이언트 생성 번호
	float dx;           // Horizontal (-1.0 ~ 1.0)
	float dz;           // Vertical (-1.0 ~ 1.0)
	PLAYER_MOVEMENT_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_MOVEMENT) {}
};

// Server -> Client: 월드 상태 동기화 (AOI 적용 대상)
struct UPDATE_PLAYER_MOVEMENT_PACKET : public PACKET_HEADER 
{
	UINT32 lastInputSeq; // 서버가 처리 완료한 해당 유저의 마지막 입력 번호
	INT64 userUUID;      // 대상 유저 고유 ID
	Vector3 currentPos;  // 서버 물리 엔진이 확정한 현재 좌표
	float currentSpeed;  // 모디파이어가 적용된 현재 실시간 속도
	bool isMoving;       // 현재 이동 여부 플래그

	UPDATE_PLAYER_MOVEMENT_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::UPDATE_PLAYER_MOVEMENT) {}
};

// Server -> Client: 유저 상태 변화 (버프/디버프 등)
struct PLAYER_STATUS_NTF_PACKET : public PACKET_HEADER 
{
	INT64 userUUID;
	float moveSpeed;     // 현재 이동 속도 수치
	UINT32 statusFlags;  // 비트마스크 (0: 정상, 1: 슬로우, 2: 스턴 등)
	PLAYER_STATUS_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_STATUS_NTF) {}
};
#pragma endregion

#pragma region Player Physics
enum class ACTION_TYPE : UINT8 { PUSH = 0, PULL = 1 };

struct PLAYER_ACTION_REQUEST_PACKET : public PACKET_HEADER
{
	ACTION_TYPE actionType;
	INT32 targetUUID;

	PLAYER_ACTION_REQUEST_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::PLAYER_ACTION_REQUEST) {}
};
#pragma endregion

#pragma region Shop Packet
//SHOP_INFO
struct SHOP_INFO_PACKET : public PACKET_HEADER
{
	INT32 currentItemID;	//현재 판매중 아이템 (1개만 할 거)
	INT64 nextUpdateTime;	//다음 갱신 시간
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
	bool isSuccess; // 성공/실패 여부

	SHOP_BUY_RESPONSE_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::SHOP_BUY_RESPONSE)
	{
		isSuccess = false;
	}
};
#pragma endregion
#pragma region Inventory Packet
//INVENTORY_INFO
struct INVENTORY_INFO_PACKET : public PACKET_HEADER
{
	INT64 userUUID;
	INT32 itemIDs[INVENTORY_SIZE] = { 0, };
	INVENTORY_INFO_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::INVENTORY_INFO) {}
};
#pragma endregion
#pragma region Trade Packets

//TRADE_REQUEST, A가 B에게 거래 요청
struct TRADE_REQUEST_PACKET : public PACKET_HEADER
{
	INT64 targetUUID;	//요청할 상대방 ID
	TRADE_REQUEST_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_REQUEST) {}
};

//TRADE_REQUEST_NTF, B에게 A의 거래 요청 알림
struct TRADE_REQUEST_NTF_PACKET : public PACKET_HEADER
{
	INT64 reqUUID;						//요청한 유저 ID
	char reqName[MAX_USER_ID_LEN + 1];	//요청한 유저 이름
	TRADE_REQUEST_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_REQUEST_NTF) {}
};

//TRADE_RESPONSE, B가 수락 / 거절함
struct TRADE_RESPONSE_PACKET : public PACKET_HEADER
{
	INT64 tradeUUID;	// 본인 B의 ID를 담아서 전송
	bool isAccept;		// 수락 / 거절
	TRADE_RESPONSE_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_RESPONSE) {}
};

struct TRADE_START_NTF_PACKET : public PACKET_HEADER
{
	INT64 tradeUUID; //거래하는 상대방 UUID
	char reqName[MAX_USER_ID_LEN + 1] = { 0, };
	TRADE_START_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_START_NTF) {}
};

//TRADE_ITEM_UPDATE, 아이템을 올리거나 뺄 때 전송됨
struct TRADE_ITEM_UPDATE_PACKET : public PACKET_HEADER
{
	INT32 tradeSlot;    // 거래창 슬롯 번호 (0 ~ 8) - UI 어디에 보여줄지
	INT32 invenSlot;    // 인벤토리 원본 슬롯 번호 (0 ~ 100) - Redis 삭제용
	INT32 itemID;       // 아이템 ID
	TRADE_ITEM_UPDATE_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_ITEM_UPDATE) {}
};

//TRADE_ITEM_NTF, 상대의 아이템 패킷(상대가 아이템 올리면 이게 옴)
struct TRADE_ITEM_NTF_PACKET : public PACKET_HEADER
{
	INT32 index;	//인벤 슬룻 번호
	INT32 itemID;	//아이템 ID
	TRADE_ITEM_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_ITEM_NTF) {}
};

//TRADE_LOCK, 교환 확정 버튼 클릭
struct TRADE_LOCK_PACKET : public PACKET_HEADER
{
	bool isLock;
	TRADE_LOCK_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_LOCK) {}
};

//TRADE_LOCK_NTF, 상대의 lock 상태 받음
struct TRADE_LOCK_NTF_PACKET : public PACKET_HEADER
{
	bool isLock;
	TRADE_LOCK_NTF_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_LOCK_NTF) {}
};

//TRADE_CONFIRM, 거래 확정 버튼 클릭
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


//TRADE_RESULT, 레디스 트랜잭션 결과
struct TRADE_RESULT_PACKET : public PACKET_HEADER
{
	bool isSuccess;	//true면 갱신, false면 실패
	TRADE_RESULT_PACKET() : PACKET_HEADER(sizeof(*this), PACKET_ID::TRADE_RESULT) {}
};
#pragma endregion

#pragma pack(pop) //위에 설정된 패킹설정이 사라짐

