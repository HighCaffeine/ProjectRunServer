#pragma once
#include <vector>
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

		// Nav 사용 X
		//const std::string navMeshFileName("all_tiles_tilecache.bin");
		//NavMeshManager::GetInstance()->Init(navMeshFileName);

		for (auto i = 0; i < maxRoomCount_; i++)
		{
			mRoomList[i] = new Room();
			mRoomList[i]->SendPacketFunc = SendPacketFunc;
			mRoomList[i]->Init((i + beginRoomNumber_), maxRoomUserCount_);
		}
	}

	UINT GetMaxRoomCount() { return mMaxRoomCount; }
		
	UINT16 EnterUser(INT32 roomNumber_, User* user_)
	{
		Room* pRoom = nullptr;

		// 1. 방 생성 요청(-1)일 경우: 인원수가 0명인 빈 방을 찾는다.
		if (roomNumber_ == -1)
		{
			int maxRooms = GetMaxRoomCount(); // 생성되어 있는 최대 방 개수
			for (int i = 0; i < maxRooms; ++i)
			{
				auto room = GetRoomByNumber(i);
				if (room != nullptr && room->GetCurrentUserCount() == 0)
				{
					pRoom = room;
					break; // 빈 방 찾았으니 탐색 종료
				}
			}
		}
		// 2. 기존 방 참가 요청인 경우
		else
		{
			pRoom = GetRoomByNumber(roomNumber_);
		}

		// 방을 못 찾았거나, 생성된 모든 방이 꽉 차서 더 이상 만들 수 없는 경우
		if (pRoom == nullptr)
		{
			return 61; // (UINT16)ERROR_CODE::ROOM_INVALID_INDEX 반환
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
