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
		int PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);
		PacketInfo packetInfo;
		packetInfo.PacketId = 0;
		packetInfo.DataSize = 0;
		packetInfo.pDataPtr = nullptr;

		UINT32 remain = mPacketDataBufferWPos - mPacketDataBufferRPos;
		if (remain < (UINT32)PACKET_HEADER_LENGTH) return packetInfo;

		auto pHeader = (PACKET_HEADER*)&mPacketDataBuffer[mPacketDataBufferRPos];

		if (pHeader->PacketLength < (UINT32)PACKET_HEADER_LENGTH || pHeader->PacketLength > 2048)
		{
			mPacketDataBufferWPos = 0;
			mPacketDataBufferRPos = 0;
			return packetInfo;
		}

		if (pHeader->PacketLength > remain) return packetInfo;

		packetInfo.PacketId = pHeader->PacketId;
		packetInfo.DataSize = pHeader->PacketLength;

		packetInfo.pDataPtr = new char[pHeader->PacketLength];
		CopyMemory(packetInfo.pDataPtr, &mPacketDataBuffer[mPacketDataBufferRPos], pHeader->PacketLength);

		mPacketDataBufferRPos += pHeader->PacketLength;

		return packetInfo;
	}

	bool HasPendingPacket()
	{
		std::lock_guard<std::mutex> guard(mLock);
		int PACKET_HEADER_LENGTH = sizeof(PACKET_HEADER);
		UINT32 remain = mPacketDataBufferWPos - mPacketDataBufferRPos;
		if (remain < (UINT32)PACKET_HEADER_LENGTH) return false;

		auto pHeader = (PACKET_HEADER*)&mPacketDataBuffer[mPacketDataBufferRPos];
		if (pHeader->PacketLength < (UINT32)PACKET_HEADER_LENGTH || pHeader->PacketLength > 2048) return false;
		if (pHeader->PacketLength > remain) return false;

		return true;
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

	void SetCharacterID(INT32 id) { mCharacterID = id; }
	INT32 GetCharacterID() { return mCharacterID; }
private:
	INT32 mCharacterID;
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

