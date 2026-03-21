#pragma once
#include "Room.h"

class RoomManager
{
public:
	RoomManager() = default;
	~RoomManager() = default;

	void Init(const INT32 beginRoomNumber_, const INT32 maxRoomCount_, const INT32 maxRoomUserCount_)
	{
		mBeginRoomNumber = beginRoomNumber_;
		mMaxRoomCount = maxRoomCount_;
		mEndRoomNumber = beginRoomNumber_ + maxRoomCount_;

		mRoomList = std::vector<Room*>(maxRoomCount_);

		// temp
		const std::string navMeshFileName("all_tiles_tilecache.bin");

		NavMeshManager::GetInstance()->Init(navMeshFileName);

		for (auto i = 0; i < maxRoomCount_; i++)
		{
			mRoomList[i] = new Room();
			mRoomList[i]->SendPacketFunc = SendPacketFunc;
			mRoomList[i]->Init((i + beginRoomNumber_), maxRoomUserCount_, navMeshFileName);
		}
	}

	UINT GetMaxRoomCount() { return mMaxRoomCount; }
		
	UINT16 EnterUser(INT32 roomNumber_, User* user_)
	{
		auto pRoom = GetRoomByNumber(roomNumber_);
		if (pRoom == nullptr)
		{
			return (UINT16)ERROR_CODE::ROOM_INVALID_INDEX;
		}


		return pRoom->EnterUser(user_);
	}
		
	INT16 LeaveUser(INT32 roomNumber_, User* user_)
	{
		auto pRoom = GetRoomByNumber(roomNumber_);
		if (pRoom == nullptr)
		{
			return (INT16)ERROR_CODE::ROOM_INVALID_INDEX;
		}
			
		user_->SetDomainState(User::DOMAIN_STATE::LOGIN);
		pRoom->LeaveUser(user_);
		return (INT16)ERROR_CODE::NONE;
	}

	Room* GetRoomByNumber(INT32 number_) 
	{ 
		if (number_ < mBeginRoomNumber || number_ >= mEndRoomNumber)
		{
			return nullptr;
		}

		auto index = (number_ - mBeginRoomNumber);
		return mRoomList[index]; 
	} 

	void SendToAllUser(const UINT16 dataSize_, char* data_, const INT32 passUserIndex_, bool exceptMe)
	{
		for (auto& room : mRoomList)
		{
			room->SendToAllUser(dataSize_, data_, passUserIndex_, exceptMe);
		}
	}

	void UpdateAllRooms(const float dt)
	{
		for (auto& room : mRoomList)
		{
			if (room != nullptr) room->Update(dt);
		}
	}

		
	std::function<void(UINT32, UINT16, char*)> SendPacketFunc;
		

private:
	std::vector<Room*> mRoomList;
	INT32 mBeginRoomNumber = 0;
	INT32 mEndRoomNumber = 0;
	INT32 mMaxRoomCount = 0;
};
