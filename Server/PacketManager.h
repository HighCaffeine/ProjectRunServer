#pragma once

#include "Packet.h"

#include <WinSock2.h>
#include <unordered_map>
#include <deque>
#include <functional>
#include <thread>
#include <mutex>
#include <map>


//대역폭 확인용
#include <atomic>


class User;
class Room;
class UserManager;
class RoomManager;
class RedisManager;

Vector3 stringToVector3(const std::string& s);



class PacketManager {
public:
	PacketManager() = default;
	~PacketManager() = default;

	void Init(const UINT32 maxClient_);

	bool Run();

	void End();

	void ReceivePacketData(const UINT32 clientIndex_, const UINT32 size_, char* pData_);

	void PushSystemPacket(PacketInfo packet_);
		
	std::function<void(UINT32, UINT32, char*)> SendPacketFunc;

	void RegisterSendFunction(std::function<void(UINT32, UINT32, char*)> sendFunc)
	{
		SendPacketFunc = [this, sendFunc](UINT32 clientIndex, UINT32 dataSize, char* pData)
			{
				// 보내는 양 누적
				m_TotalSendBytes += dataSize;

				// 실제 전송 함수 호출
				if (sendFunc) sendFunc(clientIndex, dataSize, pData);
			};
	}

private:
	void CreateCompent(const UINT32 maxClient_);

	void ClearConnectionInfo(INT32 clientIndex_);

	void EnqueuePacketData(const UINT32 clientIndex_);
	PacketInfo DequePacketData();

	PacketInfo DequeSystemPacketData();

	void RedisReqNotice(User& user, const std::string noticeMsg);


	void ProcessPacket();

	void ProcessRecvPacket(const UINT32 clientIndex_, const UINT16 packetId_, const UINT16 packetSize_, char* pPacket_);

	void ProcessUserConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessUserDisConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	
	void ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessNoticeDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	
	void ProcessEnterRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessLeaveRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessPlayerMovement(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessPlayerAction(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessRoomChatMessage(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	//인벤처리
	void ProcessInventoryDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	//거래처리
	void ProcessTradeRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessTradeResponse(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessTradeItemUpdate(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessTradeLock(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessTradeConfirm(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessTradeDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	//상점처리
	void ProcessShopUpdateDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessShopBuyRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessShopBuyDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	typedef void(PacketManager::* PROCESS_RECV_PACKET_FUNCTION)(UINT32, UINT16, char*);
	std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION> mRecvFuntionDictionary;

	//물리 처리
	bool mIsRunLogicThread = false;
	std::thread mLogicThread;
	void LogicThread(); // 20ms 주기로 실행될 함수

	// UDP 통신 관련
	SOCKET mUdpSocket = INVALID_SOCKET;
	std::thread mUdpRecvThread;
	void UDPRecvThread(); // UDP 패킷 수신 전용 함수

	// UDP 주소로 유저를 찾기 위한 맵 
	std::mutex mUdpMapLock;
	std::map<std::string, UINT32> mUdpAddrToUserIdx;

	bool UDPRun();

	//대역폭 확인
	std::atomic<uint64_t> m_TotalSendBytes{ 0 };
	std::atomic<uint64_t> m_TotalRecvBytes{ 0 };

	std::atomic<uint64_t> m_GrandTotalRecvBytes{ 0 };
	std::atomic<uint64_t> m_GrandTotalSendBytes{ 0 };


	UserManager* mUserManager;
	RoomManager* mRoomManager;	
	RedisManager* mRedisMgr;
		
	std::function<void(int, char*)> mSendMQDataFunc;
	bool mIsRunProcessThread = false;
	
	std::thread mProcessThread;
	std::mutex mLock;
	std::deque<UINT32> mInComingPacketUserIndex;
	std::deque<PacketInfo> mSystemPacketQueue;


	//Shop
	int mCurrentShopItemID = 101;
	INT64 mNextShopUpdateTime = 0;

#pragma region Trade session
	struct TradeSession
	{
		int userA, userB;	//A B의 id
		bool isLockA = false, isLockB = false;	//lock상태
		bool isConfirmA = false, isConfirmB = false;	//confirm상태
		std::vector<int> itemsA, itemsB;	//올린 아이템들
		std::vector<int> itemsASlot, itemsBSlot;
	};
	TradeSession curTS;
#pragma endregion
};

