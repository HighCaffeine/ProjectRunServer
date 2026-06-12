#pragma once

#include "NetworkFramework\IOCPServer.h"
#include "Lobby_PacketManager.h"
#include "Packet\Packet.h"

#include <vector>
#include <deque>
#include <memory>
#include <thread>
#include <mutex>

class LobbyServer : public IOCPServer
{
public:
	LobbyServer() = default;
	virtual ~LobbyServer() = default;

	virtual void OnConnect(const UINT32 clientIndex_) override
	{
		printf("[OnConnect] 클라이언트: Index(%d)\n", clientIndex_);

		PacketInfo packet{ clientIndex_, (UINT16)PACKET_ID::SYS_USER_CONNECT, 0 };
		m_pPacketManager->PushSystemPacket(packet);
	}

	virtual void OnClose(const UINT32 clientIndex_) override
	{
		printf("[OnClose] 클라이언트: Index(%d)\n", clientIndex_);

		PacketInfo packet{ clientIndex_, (UINT16)PACKET_ID::SYS_USER_DISCONNECT, 0 };
		m_pPacketManager->PushSystemPacket(packet);
	}

	/*virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) override
	{
		m_pPacketManager->ReceivePacketData(clientIndex_, size_, pData_);
	}*/

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) override
	{
		printf("[OnReceive] Index(%d), size(%d)\n", clientIndex_, size_);

		if (size_ >= sizeof(PACKET_HEADER))
		{
			auto pHeader = reinterpret_cast<PACKET_HEADER*>(pData_);
			if (pHeader->PacketId == (UINT16)PACKET_ID::SYS_TIME_SYNC_REQ)
			{
				m_pPacketManager->ProcessTimeSync(clientIndex_, size_, pData_);
				return;
			}
		}
		m_pPacketManager->ReceivePacketData(clientIndex_, size_, pData_);
	}

	void Run(const UINT32 maxClient)
	{
		m_pPacketManager = std::make_unique<PacketManager>(); // Lobby_PacketManager 

		// 네트워크 전송 콜백 등록 (전송량 검사 포함)
		m_pPacketManager->RegisterSendFunction([&](UINT32 clientIndex_, UINT32 packetSize, char* pSendPacket) {
			SendMsg(clientIndex_, packetSize, pSendPacket);
			});

		m_pPacketManager->Init(maxClient);
		m_pPacketManager->Run();

		StartServer(maxClient);
	}

	void End()
	{
		m_pPacketManager->End();
		DestroyThread();
	}

private:
	std::unique_ptr<PacketManager> m_pPacketManager;
};