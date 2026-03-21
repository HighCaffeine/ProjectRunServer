#pragma once

#include "RedisTaskDefine.h"
//#include "ErrorCode.h"

//#include "../thirdparty/CRedisConn.h"
#include "CRedisConnEx.h"
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <random>
#include <iostream>
#include <algorithm>
#include <chrono>

class RedisManager
{
public:
	RedisManager() = default;
	~RedisManager() = default;

	bool Run(std::string ip_, UINT16 port_, const UINT32 threadCount_)
	{
		if (Connect(ip_, port_) == false)
		{
			printf("RedisManager::Run() Redis 접속 실패\n");
			return false;
		}

		mIsTaskRun = true;

		for (UINT32 i = 0; i < threadCount_; i++)
		{
			mTaskThreads.emplace_back([this]() { TaskProcessThread(); });
		}

		// Redis Sub 용 Thread
		mTaskThreads.emplace_back([this]() { SubscribeThread(); });

		printf("RedisManager::Run() Redis 동작 중...\n");
		return true;
	}

	void End()
	{
		mIsTaskRun = false;

		mConnSub.disConnect();

		for (auto& thread : mTaskThreads)
		{
			if (thread.joinable())
			{
				thread.join();
			}
		}
	}

	void PushTask(RedisTask task_)
	{
		std::lock_guard<std::mutex> guard(mReqLock);
		mRequestTask.push_back(task_);
	}

	RedisTask TakeResponseTask()
	{
		std::lock_guard<std::mutex> guard(mResLock);

		if (mResponseTask.empty())
		{
			return RedisTask();
		}

		auto task = mResponseTask.front();
		mResponseTask.pop_front();

		return task;
	}


private:
	bool Connect(std::string ip_, UINT16 port_)
	{
		if (mConn.connect(ip_, port_) == false)
		{
			std::cout << "RedisManager::Connect() Redis connect error " << mConn.getErrorStr() << std::endl;
			return false;
		}
		else
		{
			std::cout << "RedisManager::Connect() Redis connect success !!!" << std::endl;
		}

		if (mConnSub.connect(ip_, port_) == false)
		{
			std::cout << "RedisManager::Connect() Redis(Sub) connect error " << mConn.getErrorStr() << std::endl;
			return false;
		}
		else
		{
			std::cout << "RedisManager::Connect() Redis(Sub) connect success !!!" << std::endl;
		}

		return true;
	}

	void TaskProcessThread()
	{
		printf("RedisManager::TaskProcessThread() Redis 스레드 시작...\n");

		while (mIsTaskRun)
		{
			bool isIdle = true;

			if (auto task = TakeRequestTask(); task.TaskID != RedisTaskID::INVALID)
			{
				isIdle = false;
				static bool isBuying = false;

				if (task.TaskID == RedisTaskID::REQUEST_LOGIN)
				{
					auto pRequest = (RedisLoginReq*)task.pData;

					RedisLoginRes bodyData;
					memset(&bodyData, 0, sizeof(RedisLoginRes));
					bodyData.Result = (UINT16)ERROR_CODE::LOGIN_USER_INVALID_PW;

					std::string value;
					if (mConn.get(pRequest->UserID, value))
					{
						if (value.compare(pRequest->UserPW) == 0)
						{
							bodyData.Result = (UINT16)ERROR_CODE::NONE;
							CopyUserID(bodyData.UserID, pRequest->UserID);
						}
						else
						{
							printf("[Redis] PW Mismatch. Req:%s / DB:%s\n", pRequest->UserPW, value.c_str());
						}
					}
					else
					{
						// 계정이 없는 경우 (필요하면 여기서 자동 가입 로직 추가)
						printf("[Redis] User Not Found: %s\n", pRequest->UserID);
					}

					RedisTask resTask;
					resTask.UserIndex = task.UserIndex;
					resTask.TaskID = RedisTaskID::RESPONSE_LOGIN;
					resTask.DataSize = sizeof(RedisLoginRes);
					resTask.pData = new char[resTask.DataSize];
					CopyMemory(resTask.pData, (char*)&bodyData, resTask.DataSize);

					PushResponse(resTask);
				}
				else if (task.TaskID == RedisTaskID::REQUEST_NOTICE)
				{
					auto pRequest = (RedisNoticeReq*)task.pData;

					mConn.publish("ch_notice", pRequest->Message);
					
				}
				else if (task.TaskID == RedisTaskID::REQUEST_LOAD_INVENTORY)
				{
					auto pRequest = (RedisInvenReq*)task.pData;
					RedisInvenRes resData;

					resData.UserIndex = pRequest->UserIndex;
					memset(resData.ItemSlots, 0, sizeof(resData.ItemSlots));

					//레디스 해쉬 키값
					std::string id = "u:" + std::string(pRequest->UserID) + ":inven";
					std::cout << id << std::endl;
					std::map<std::string, std::string> inven;

					//getall로 가져오고, 안에 안비었으면 내부 처리
					if (mConn.hgetall(id, inven) && !inven.empty())
					{	
						//페어로 값 가져옴
						//내부에 0 100 1 200 2 300 같이 저장할거 (키를 인덱스로 바로 쓸 수 있도록)
						for (auto const& [key, value] : inven)
						{
							int index = std::stoi(key);
							if (index >= 0 && index < INVENTORY_SIZE)
							{
								//아이템 하나씩 세팅 (빈칸은 0)
								resData.ItemSlots[index] = std::stoi(value);
							}
						}

					}
					else //인벤이 없다면 (신규 유저라면)
					{
						//랜덤 아이템 ID 값 가져옴
						int item1 = 101 + (rand() % 5);
						int item2 = 101 + (rand() % 5);
						uint32_t ret;

						//랜덤 인덱스 얻기
						static std::random_device rd;
						static std::mt19937 gen(rd());
						int nums[] = {0, 1, 2, 3, 4};
						int first = -1, firstIndex = -1;
						int sec = -1, secIndex = -1;
						
						//랜덤 인덱스 1
						std::uniform_int_distribution<int> dis1(0, 4); firstIndex = dis1(gen);
						first = nums[firstIndex];

						//맨 뒤랑 교체
						std::swap(nums[firstIndex], nums[4]);

						//랜덤 인덱스 2
						std::uniform_int_distribution<int> dis2(0, 3); secIndex = dis2(gen);
						sec = nums[secIndex];

						//다 0으로 세팅
						for (int i = 0; i < INVENTORY_SIZE; i++)
						{
							mConn.hset(id, std::to_string(i), "0", ret);
						}

						mConn.hset(id, std::to_string(first), std::to_string(item1), ret);
						mConn.hset(id, std::to_string(sec), std::to_string(item2), ret);

						resData.ItemSlots[first] = item1;
						resData.ItemSlots[sec] = item2;
					}
				
					//결과 반환해줌
					RedisTask resTask;
					resTask.TaskID = RedisTaskID::RESPONSE_LOAD_INVENTORY;
					resTask.UserIndex = task.UserIndex;
					resTask.DataSize = sizeof(RedisInvenRes);
					resTask.pData = new char[resTask.DataSize];
					memcpy(resTask.pData, &resData, resTask.DataSize);

					PushResponse(resTask);
				}
#pragma region Trade



				//else if (task.TaskID == RedisTaskID::REQUEST_TRADE_EXCHANGE)
				//{
				//	auto pRequest = (RedisTradeReq*)task.pData;
				//	RedisTradeRes resData;
				//	//유저 키(해쉬 키)
				//	std::string Aid = "u:" + std::string(pRequest->UserAID) + ":inven";
				//	std::string Bid = "u:" + std::string(pRequest->UserBID) + ":inven";
				//	std::map<std::string, std::string> invenA;
				//	std::map<std::string, std::string> invenB;
				//	std::deque<int> exchangeQueueA;
				//	std::deque<int> exchangeQueueB;

				//	uint32_t ret;
				//	if (mConn.hgetall(Aid, invenA) && mConn.hgetall(Bid, invenB))
				//	{
				//		int arrayA[INVENTORY_SIZE]; // 현재 가지고있는 인벤토리 데이터
				//		int arrayB[INVENTORY_SIZE];

				//		std::fill(arrayA, arrayA + INVENTORY_SIZE, EMPTYITEM);
				//		std::fill(arrayB, arrayB + INVENTORY_SIZE, EMPTYITEM);

				//		for (int i = 0; i < INVENTORY_SIZE; i++) // 교환창에있는 아이템들을 Queue에 담음
				//		{
				//			if (pRequest->ItemsAID[i] != EMPTYITEM) exchangeQueueA.push_back(pRequest->ItemsAID[i]); 
				//			if (pRequest->ItemsBID[i] != EMPTYITEM) exchangeQueueB.push_back(pRequest->ItemsBID[i]);
				//		}

				//		// 현재 가지고있는 아이템들을 DB에서 가져오기
				//		for (auto& [key, value] : invenA)
				//		{

				//			int index = std::stoi(key);
				//			if (index < INVENTORY_SIZE && index >= 0)
				//			{
				//				arrayA[index] = std::stoi(value);
				//			}
				//		}

				//		for (auto& [key, value] : invenB)
				//		{
				//			int index = std::stoi(key);

				//			if (index < INVENTORY_SIZE && index >= 0)
				//			{
				//				arrayB[index] = std::stoi(value);
				//			}
				//		}

				//		for (int i = 0; i < INVENTORY_SIZE; i++) 
				//		{
				//			if (pRequest->ItemsASlot[i] >= 0 && pRequest->ItemsASlot[i] < INVENTORY_SIZE) 
				//			{
				//				if (pRequest->ItemsAID[i] != EMPTYITEM) 
				//				{
				//					arrayA[pRequest->ItemsASlot[i]] = EMPTYITEM;
				//				}
				//			}
				//			if (pRequest->ItemsBSlot[i] >= 0 && pRequest->ItemsBSlot[i] < INVENTORY_SIZE) 
				//			{
				//				if (pRequest->ItemsBID[i] != EMPTYITEM) 
				//				{
				//					arrayB[pRequest->ItemsBSlot[i]] = EMPTYITEM;
				//				}
				//			}
				//		}

				//		for (int i = 0; i < INVENTORY_SIZE; i++) 
				//		{
				//			if (arrayA[i] == EMPTYITEM && !exchangeQueueB.empty()) 
				//			{
				//				arrayA[i] = exchangeQueueB.front();
				//				exchangeQueueB.pop_front();
				//			}
				//			if (arrayB[i] == EMPTYITEM && !exchangeQueueA.empty()) 
				//			{
				//				arrayB[i] = exchangeQueueA.front();
				//				exchangeQueueA.pop_front();
				//			}
				//		}

				//		if (!exchangeQueueA.empty() || !exchangeQueueB.empty()) // 슬롯이 꽉 차 교환할 수 없음
				//		{
				//			resData.IsSuccess = false;
				//			RedisTask resTaskA;
				//			resTaskA.TaskID = RedisTaskID::RESPONSE_TRADE_EXCHANGE;
				//			resTaskA.UserIndex = pRequest->UserA;
				//			resTaskA.DataSize = sizeof(RedisTradeRes);
				//			resTaskA.pData = new char[resTaskA.DataSize];
				//			memcpy(resTaskA.pData, &resData, resTaskA.DataSize);
				//			PushResponse(resTaskA);

				//			RedisTask resTaskB;
				//			resTaskB.TaskID = RedisTaskID::RESPONSE_TRADE_EXCHANGE;
				//			resTaskB.UserIndex = pRequest->UserB;
				//			resTaskB.DataSize = sizeof(RedisTradeRes);
				//			resTaskB.pData = new char[resTaskB.DataSize];
				//			memcpy(resTaskB.pData, &resData, resTaskB.DataSize);
				//			PushResponse(resTaskB);
				//		}
				//		else // 실제 DB적용
				//		{
				//			redisReply* reply = mConn.redisCmd("MULTI");
				//			freeReplyObject(reply);

				//			for (int i = 0; i < INVENTORY_SIZE; i++)
				//			{
				//				mConn.redisCmd("HSET %s %d %d", Aid.c_str(), i, arrayA[i]);
				//				mConn.redisCmd("HSET %s %d %d", Bid.c_str(), i, arrayB[i]);
				//			}

				//			reply = mConn.redisCmd("EXEC");
				//			freeReplyObject(reply);

				//			resData.IsSuccess = true;
				//			RedisTask resTaskA;
				//			resTaskA.TaskID = RedisTaskID::RESPONSE_TRADE_EXCHANGE;
				//			resTaskA.UserIndex = pRequest->UserA;
				//			resTaskA.DataSize = sizeof(RedisTradeRes);
				//			resTaskA.pData = new char[resTaskA.DataSize];
				//			memcpy(resTaskA.pData, &resData, resTaskA.DataSize);
				//			PushResponse(resTaskA);

				//			RedisTask resTaskB;
				//			resTaskB.TaskID = RedisTaskID::RESPONSE_TRADE_EXCHANGE;
				//			resTaskB.UserIndex = pRequest->UserB;
				//			resTaskB.DataSize = sizeof(RedisTradeRes);
				//			resTaskB.pData = new char[resTaskB.DataSize];
				//			memcpy(resTaskB.pData, &resData, resTaskB.DataSize);
				//			PushResponse(resTaskB);
				//		}
				//		
				//	}
				//}
#pragma endregion
				else if (task.TaskID == RedisTaskID::REQUEST_SHOP_UPDATE)
				{
					int commandValue = 0; // 0이면 바로 초기화, 1이상이면 시간 추가 및 체크, -1은 processpacket에서 1초마다 체크용
					if (task.DataSize == sizeof(int) && task.pData != nullptr)
					{
						commandValue = *(int*)task.pData;
					}

					time_t now = std::time(nullptr);
					std::string dbTime, dbItem;
					UINT64 storedNextTime = 0;
					int currentItemID = 101;


					mConn.hget("game:shop_state", "next_update_ts", dbTime);
					mConn.hget("game:shop_state", "current_item", dbItem);

					if (!dbTime.empty()) storedNextTime = std::stoull(dbTime);
					if (!dbItem.empty()) currentItemID = std::stoi(dbItem);

					// 자정 초기화를 위해 날짜 계산
					auto GetNextMidnight = [&](time_t baseTime)->UINT64 
						{
						struct tm timeInfo;
						localtime_s(&timeInfo, &baseTime); // 현재 시간 구조체로 변환

						timeInfo.tm_hour = 0;
						timeInfo.tm_min = 0;
						timeInfo.tm_sec = 0;
						timeInfo.tm_mday += 1; // 날짜 하루 더함 (자동으로 월/년 넘어감)

						return (UINT64)mktime(&timeInfo); // 다시 타임스탬프로 변환
						};

					bool needDBUpdate = false;
					UINT64 finalNextTime = storedNextTime;

					
					// /shop_reset 그냥 초기화
					if (commandValue == 0)
					{
						currentItemID = 101 + (rand() % 5);
						finalNextTime = GetNextMidnight(now); // 지금 기준으로 내일 자정 계산
						needDBUpdate = true;
						printf("[Shop] Reset\n");
					}
					else if (commandValue > 0)
					{
						UINT64 reduceSeconds = (UINT64)commandValue * 60;

						if (storedNextTime <= (UINT64)now + reduceSeconds)
						{
							finalNextTime = (UINT64)now - 1;
						}
						else
						{
							finalNextTime = storedNextTime - reduceSeconds;
						}

						needDBUpdate = true;
						printf("[Shop] Time Fast Forward: %d mins (Next Update: %lld)\n", commandValue, finalNextTime);
					}
					else if (commandValue == -1)	//시간 체크 processpacket에서 요청
					{
						if (isBuying)
						{
							isBuying = false;
							needDBUpdate = true;
						}
						if (storedNextTime == 0 || (UINT64)now >= storedNextTime)
						{
							currentItemID = 101 + (rand() % 5); // 아이템 변경
							finalNextTime = GetNextMidnight(now); // 내일 자정으로 다시 갱신함

							needDBUpdate = true;
							printf("[Shop] Daily Reset\n");
						}
					}
					else if (commandValue == -2)	//무조건 업데이트
					{
						needDBUpdate = true;
					}

					// 변경사항 저장, 브로드캐스트
					if (needDBUpdate)
					{
						uint32_t ret;
						mConn.hset("game:shop_state", "current_item", std::to_string(currentItemID), ret);
						mConn.hset("game:shop_state", "next_update_ts", std::to_string(finalNextTime), ret);
					}

					if (needDBUpdate || commandValue != -1)
					{
						RedisShopRes resData;
						resData.ItemID = currentItemID;
						resData.NextUpdateTime = finalNextTime;

						RedisTask resTask;
						resTask.TaskID = RedisTaskID::RESPONSE_SHOP_UPDATE;
						resTask.UserIndex = 0;
						resTask.DataSize = sizeof(RedisShopRes);
						resTask.pData = new char[resTask.DataSize];
						memcpy(resTask.pData, &resData, resTask.DataSize);

						PushResponse(resTask);
					}
				}
				else if (task.TaskID == RedisTaskID::REQUEST_SHOP_BUY)
				{
					auto pRequest = (RedisShopBuyReq*)task.pData;
					RedisShopBuyRes resData;
					resData.isSuccess = false;

					// 유저 인벤토리 조회
					std::string invenKey = "u:" + std::string(pRequest->UserID) + ":inven";
					std::map<std::string, std::string> inven;

					mConn.hgetall(invenKey, inven);

					// 빈 슬롯 찾기
					int emptySlotIndex = -1;
					int tempInven[INVENTORY_SIZE] = { 0, };

					for (auto const& [key, value] : inven)
					{
						int idx = std::stoi(key);
						if (idx >= 0 && idx < INVENTORY_SIZE)
						{
							tempInven[idx] = std::stoi(value);
						}
					}

					for (int i = 0; i < INVENTORY_SIZE; i++)
					{
						if (tempInven[i] == 0)
						{
							emptySlotIndex = i;
							break;
						}
					}

					if (emptySlotIndex != -1)
					{
						uint32_t ret;
						mConn.hset(invenKey, std::to_string(emptySlotIndex), std::to_string(pRequest->itemID), ret);

						mConn.hset("game:shop_state", "current_item", "0", ret);

						resData.isSuccess = true;
						isBuying = true;
						printf("[Shop] User(%s) Buy Item(%d) at Slot(%d)\n", pRequest->UserID, pRequest->itemID, emptySlotIndex);
					}
					else
					{
						// 인벤토리 꽉 참
						resData.isSuccess = false;
						printf("[Shop] User(%s) Buy Failed (Inventory Full)\n", pRequest->UserID);
					}

					RedisTask resTask;
					resTask.TaskID = RedisTaskID::RESPONSE_SHOP_BUY;
					resTask.UserIndex = task.UserIndex; // 요청한 유저에게만 전송
					resTask.DataSize = sizeof(RedisShopBuyRes);
					resTask.pData = new char[resTask.DataSize];
					memcpy(resTask.pData, &resData, resTask.DataSize);
					PushResponse(resTask);

					// 성공 시 인벤토리 자동 갱신 요청
					if (resData.isSuccess)
					{
						RedisInvenReq invenReq;
						invenReq.UserIndex = task.UserIndex;
						strncpy_s(invenReq.UserID, MAX_USER_ID_LEN + 1, pRequest->UserID, _TRUNCATE);

						RedisTask invenTask;
						invenTask.TaskID = RedisTaskID::REQUEST_LOAD_INVENTORY;
						invenTask.UserIndex = task.UserIndex;
						invenTask.DataSize = sizeof(RedisInvenReq);
						invenTask.pData = new char[invenTask.DataSize];
						memcpy(invenTask.pData, &invenReq, invenTask.DataSize);

						PushTask(invenTask);
					}
				}

				task.Release();
			}

	
			if (isIdle)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}

		printf("Redis 스레드 종료\n");
	}

	void SubscribeThread()
	{
		printf("RedisManager::SubscribeThread() Redis(Sub) 스레드 시작...\n");

		auto result = mConnSub.initSubscribe("ch_notice");

		while (mIsTaskRun)
		{
			std::string message; /* output */
			mConnSub.subscribe(message);

			RedisNoticeRes bodyData;
			CopyUserID(bodyData.UserID, "[GM]");
			CopyMemory(bodyData.Message, message.c_str(), sizeof(bodyData.Message));

			RedisTask resTask;
			resTask.UserIndex = 0; // to all users
			resTask.TaskID = RedisTaskID::RESPONSE_NOTICE;
			resTask.DataSize = sizeof(RedisNoticeRes);
			resTask.pData = new char[resTask.DataSize];
			CopyMemory(resTask.pData, (char*)&bodyData, resTask.DataSize);

			PushResponse(resTask);
		}
	}

	RedisTask TakeRequestTask()
	{
		std::lock_guard<std::mutex> guard(mReqLock);

		if (mRequestTask.empty())
		{
			return RedisTask();
		}

		auto task = mRequestTask.front();
		mRequestTask.pop_front();

		return task;
	}

	void PushResponse(RedisTask task_)
	{
		std::lock_guard<std::mutex> guard(mResLock);
		mResponseTask.push_back(task_);
	}




	private:

	RedisCpp::CRedisConnEx mConn;
	RedisCpp::CRedisConnEx mConnSub; // Redis Subscribe용

	bool		mIsTaskRun = false;
	std::vector<std::thread> mTaskThreads;

	std::mutex mReqLock;
	std::deque<RedisTask> mRequestTask;

	std::mutex mResLock;
	std::deque<RedisTask> mResponseTask;
};