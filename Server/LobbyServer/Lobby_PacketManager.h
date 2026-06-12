#pragma once

#include "Packet\Packet.h"
#include "Utility\unity.h"
#include "Packet\ErrorCode.h"

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
class UserManager;
class RedisManager;
class LobbyManager; 
class RoomManager;
class Room;

Vector3 stringToVector3(const std::string& s);

class PacketManager 
{
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

	void ProcessTimeSync(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
private:
	void CreateCompent(const UINT32 maxClient_);

	void ClearConnectionInfo(INT32 clientIndex_);

	void EnqueuePacketData(const UINT32 clientIndex_);
	PacketInfo DequePacketData();

	PacketInfo DequeSystemPacketData();

	typedef void(PacketManager::* PROCESS_RECV_PACKET_FUNCTION)(UINT32, UINT16, char*);
	std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION> mRecvFuntionDictionary;

	void RedisReqNotice(User& user, const std::string noticeMsg);

	void ProcessPacket();

	void ProcessRecvPacket(const UINT32 clientIndex_, const UINT16 packetId_, const UINT16 packetSize_, char* pPacket_);
	void ProcessUserConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessUserDisConnect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	void ProcessLogin(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessLoginDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessNoticeDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	void ProcessGameStartRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	void ProcessEnterRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessLeaveRoom(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessRoomListRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	void ProcessPlayerReady(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessCharSelect(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);

	void ProcessRoomChatMessage(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);


//거래 & 상점 패킷처리 안써서 비활성화
#if 0
	//인벤처리
	void ProcessInventoryDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);


	void ProcessTradeRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessTradeResponse(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessTradeItemUpdate(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessTradeLock(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessTradeConfirm(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessTradeDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessShopBuyRequest(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessShopBuyDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
	void ProcessShopUpdateDBResult(UINT32 clientIndex_, UINT16 packetSize_, char* pPacket_);
#endif

	//대역폭 확인
	std::atomic<uint64_t> m_TotalSendBytes{ 0 };
	std::atomic<uint64_t> m_TotalRecvBytes{ 0 };

	std::atomic<uint64_t> m_GrandTotalRecvBytes{ 0 };
	std::atomic<uint64_t> m_GrandTotalSendBytes{ 0 };


	UserManager* mUserManager;
	RoomManager* mRoomManager;
	RedisManager* mRedisMgr;
	LobbyManager* mLobbyManager;

	std::function<void(int, char*)> mSendMQDataFunc;
	bool mIsRunProcessThread = false;

	std::thread mProcessThread;
	std::mutex mLock;
	std::deque<UINT32> mInComingPacketUserIndex;
	std::deque<PacketInfo> mSystemPacketQueue;
};