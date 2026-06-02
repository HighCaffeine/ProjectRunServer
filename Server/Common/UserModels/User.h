#pragma once

#include <WinSock2.h>
#include <unordered_set>
#include <mutex>
#include <vector>

#include "Actor.h"
#include "Utility\unity.h"

class User: public Actor
{
	const UINT32 PACKET_DATA_BUFFER_SIZE = 65536;

public:

	User() = default;
	~User() = default;

	void Init(const INT32 index)
	{
		Actor::Init(index);
		mPacketDataBuffer.resize(PACKET_DATA_BUFFER_SIZE);
	}

	void Clear()
	{
		Actor::Clear();
		mIsConfirm = false;

		mPacketDataBufferWPos = 0;
		mPacketDataBufferRPos = 0;
	}

		
	void SetPacketData(const UINT32 dataSize_, char* pData_)
	{
		std::lock_guard<std::mutex> guard(mLock);
		if ((mPacketDataBufferWPos + dataSize_) >= PACKET_DATA_BUFFER_SIZE)
		{
			auto remainDataSize = mPacketDataBufferWPos - mPacketDataBufferRPos;

			if (remainDataSize > 0)
			{
				memmove(&mPacketDataBuffer[0], &mPacketDataBuffer[mPacketDataBufferRPos], remainDataSize);
				mPacketDataBufferWPos = remainDataSize;
				mPacketDataBufferRPos = 0;
			}
			else
			{
				mPacketDataBufferWPos = 0;
				mPacketDataBufferRPos = 0;
			}
		}

		if ((mPacketDataBufferWPos + dataSize_) > PACKET_DATA_BUFFER_SIZE)
		{
			printf("[Critical Error] Packet Buffer Overflow! Dropping Data.\\n");
			return; 
		}

		CopyMemory(&mPacketDataBuffer[mPacketDataBufferWPos], pData_, dataSize_);
		mPacketDataBufferWPos += dataSize_;
	}

	PacketInfo GetPacket()
	{
		std::lock_guard<std::mutex> guard(mLock);
		const int PACKET_HEADER_LENGTH = 4;
		UINT32 remainByte = mPacketDataBufferWPos - mPacketDataBufferRPos;

		if(remainByte < PACKET_HEADER_LENGTH)
		{
			return PacketInfo();
		}

		auto pHeader = (PACKET_HEADER*)&mPacketDataBuffer[mPacketDataBufferRPos];

		if (pHeader->PacketLength < PACKET_HEADER_LENGTH || pHeader->PacketLength > 2048)
		{
			printf("[Critical] TCP Stream Corrupted! (Length: %d). Clearing Buffer.\n", pHeader->PacketLength);
			mPacketDataBufferWPos = 0;
			mPacketDataBufferRPos = 0;
			return PacketInfo();
		}

		if (pHeader->PacketLength > remainByte)
		{
			return PacketInfo();
		}

		PacketInfo packetInfo;
		packetInfo.PacketId = pHeader->PacketId;
		packetInfo.DataSize = pHeader->PacketLength;
		packetInfo.pDataPtr = &mPacketDataBuffer[mPacketDataBufferRPos];
		mPacketDataBufferRPos += pHeader->PacketLength;

		return packetInfo;
	}

	void SetInventory(int index, int itemID)
	{
		if (0 <= index && index < INVENTORY_SIZE)
		{
			mInventory[index] = itemID;
		}
	}

	int GetItemID(int index)
	{
		if (0 <= index && index < INVENTORY_SIZE)
		{
			return mInventory[index];
		}

		return 0;
	}

	void SetUDPAddr(sockaddr_in addr) 
	{
		udpAddr = addr;
		isUdpActive = true;
	}

	bool isUdpActive = false;
	std::unordered_set<INT32> mVisibleList;

	void SetPing(INT32 ping) { mPing = ping; }
	INT32 GetPing() { return mPing; }
private:
	INT32 mPing = 0;

	sockaddr_in udpAddr;
	std::mutex mLock;

	bool mIsConfirm = false;
	std::string mAuthToken;

	int mInventory[INVENTORY_SIZE] = { 0, };
	

	std::vector<char> mPacketDataBuffer;
	UINT32 mPacketDataBufferWPos = 0;
	UINT32 mPacketDataBufferRPos = 0;
	char* mPakcetDataBuffer = nullptr;
};

