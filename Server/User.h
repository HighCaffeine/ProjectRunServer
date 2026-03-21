#pragma once

#include <WinSock2.h>
#include <unordered_set>
#include <mutex>

#include "Actor.h"
#include "unity.h"

class User: public Actor
{
	const UINT32 PACKET_DATA_BUFFER_SIZE = 8096;

public:

	User() = default;
	~User() = default;

	void Init(const INT32 index)
	{
		Actor::Init(index);
		mPakcetDataBuffer = new char[PACKET_DATA_BUFFER_SIZE];
	}

	void Clear()
	{
		Actor::Clear();
		mIsConfirm = false;

		mPakcetDataBufferWPos = 0;
		mPakcetDataBufferRPos = 0;
	}

		
	//TODO SetPacketData, GetPacket 함수를 멀티스레드에 호출하고 있다면 공유변수에 lock을 걸어야 한다
	void SetPacketData(const UINT32 dataSize_, char* pData_)
	{
		std::lock_guard<std::mutex> guard(mLock);
		if ((mPakcetDataBufferWPos + dataSize_) >= PACKET_DATA_BUFFER_SIZE)
		{
			auto remainDataSize = mPakcetDataBufferWPos - mPakcetDataBufferRPos;

			if (remainDataSize > 0)
			{
				CopyMemory(&mPakcetDataBuffer[0], &mPakcetDataBuffer[mPakcetDataBufferRPos], remainDataSize);
				mPakcetDataBufferWPos = remainDataSize;
			}
			else
			{
				mPakcetDataBufferWPos = 0;
			}
			
			mPakcetDataBufferRPos = 0;
		}

		CopyMemory(&mPakcetDataBuffer[mPakcetDataBufferWPos], pData_, dataSize_);
		mPakcetDataBufferWPos += dataSize_;
	}

	PacketInfo GetPacket()
	{
		std::lock_guard<std::mutex> guard(mLock);
		const int PACKET_SIZE_LENGTH = 2;
		const int PACKET_TYPE_LENGTH = 2;
		short packetSize = 0;
		
		UINT32 remainByte = mPakcetDataBufferWPos - mPakcetDataBufferRPos;

		if(remainByte < PACKET_HEADER_LENGTH)
		{
			return PacketInfo();
		}

		auto pHeader = (PACKET_HEADER*)&mPakcetDataBuffer[mPakcetDataBufferRPos];
		
		if (pHeader->PacketLength > remainByte)
		{
			return PacketInfo();
		}

		PacketInfo packetInfo;
		packetInfo.PacketId = pHeader->PacketId;
		packetInfo.DataSize = pHeader->PacketLength;
		packetInfo.pDataPtr = &mPakcetDataBuffer[mPakcetDataBufferRPos];
		
		mPakcetDataBufferRPos += pHeader->PacketLength;

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
private:


	sockaddr_in udpAddr;
	std::mutex mLock;

	bool mIsConfirm = false;
	std::string mAuthToken;

	int mInventory[INVENTORY_SIZE] = { 0, };
	

	UINT32 mPakcetDataBufferWPos = 0;
	UINT32 mPakcetDataBufferRPos = 0;
	char* mPakcetDataBuffer = nullptr;
};

