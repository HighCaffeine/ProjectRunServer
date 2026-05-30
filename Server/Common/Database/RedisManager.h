#pragma once

#include "RedisTaskDefine.h"
#include "CRedisConnEx.h"
#include "..\..\..\thirdparty\CRedisConn.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <random>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <ctime>

class RedisManager
{
public:
    RedisManager() = default;
    ~RedisManager() = default;

    bool Run(const std::string& ip, uint16_t port, uint32_t threadCount)
    {
        if (!Connect(ip, port))
        {
            printf("RedisManager::Run() Redis 접속 실패\n");
            return false;
        }

        mIsTaskRun = true;

        for (uint32_t i = 0; i < threadCount; i++)
            mTaskThreads.emplace_back([this]() { TaskProcessThread(); });

        // Redis Subscribe 전용 스레드
        mTaskThreads.emplace_back([this]() { SubscribeThread(); });

        printf("RedisManager::Run() Redis 동작 중...\n");
        return true;
    }

    void End()
    {
        mIsTaskRun = false;
        mConnSub.disConnect();

        for (auto& t : mTaskThreads)
            if (t.joinable()) t.join();
    }

    void PushTask(RedisTask task)
    {
        std::lock_guard<std::mutex> guard(mReqLock);
        mRequestTask.push_back(task);
    }

    RedisTask TakeResponseTask()
    {
        std::lock_guard<std::mutex> guard(mResLock);
        if (mResponseTask.empty()) return RedisTask();
        auto task = mResponseTask.front();
        mResponseTask.pop_front();
        return task;
    }

    void PushResponse(RedisTask task)
    {
        std::lock_guard<std::mutex> guard(mResLock);
        mResponseTask.push_back(task);
    }

private:
    bool Connect(const std::string& ip, uint16_t port)
    {
        if (!mConn.connect(ip, port))
        {
            std::cout << "RedisManager::Connect() error: " << mConn.getErrorStr() << "\n";
            return false;
        }
        std::cout << "RedisManager::Connect() success\n";

        if (!mConnSub.connect(ip, port))
        {
            std::cout << "RedisManager::Connect() Sub error: " << mConn.getErrorStr() << "\n";
            return false;
        }
        std::cout << "RedisManager::Connect() Sub success\n";
        return true;
    }

    RedisTask TakeRequestTask()
    {
        std::lock_guard<std::mutex> guard(mReqLock);
        if (mRequestTask.empty()) return RedisTask();
        auto task = mRequestTask.front();
        mRequestTask.pop_front();
        return task;
    }

    // ----------------------------------------------------------------
    //  태스크 처리 스레드
    // ----------------------------------------------------------------
    void TaskProcessThread()
    {
        printf("RedisManager::TaskProcessThread() 시작\n");

        static bool isBuying = false;   // 상점 거래 중 플래그 (미사용이지만 코드 유지)

        while (mIsTaskRun)
        {
            bool isIdle = true;
            auto task = TakeRequestTask();

            if (task.TaskID != RedisTaskID::INVALID)
            {
                isIdle = false;
                ProcessTask(task, isBuying);
                task.Release();
            }

            if (isIdle)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        printf("RedisManager::TaskProcessThread() 종료\n");
    }

    void ProcessTask(RedisTask& task, bool& isBuying)
    {
        switch (task.TaskID)
        {
            // ------------------------------------------------------------
        case RedisTaskID::REQUEST_LOGIN:
        {
            auto* req = reinterpret_cast<RedisLoginReq*>(task.pData);

            RedisLoginRes body{};
            body.Result = static_cast<uint16_t>(ERROR_CODE::LOGIN_USER_INVALID_PW);

            std::string value;
            if (mConn.get(req->UserID, value))
            {
                if (value == req->UserPW)
                {
                    body.Result = static_cast<uint16_t>(ERROR_CODE::NONE);
                    CopyUserID(body.UserID, req->UserID);
                }
                else
                {
                    printf("[Redis] PW Mismatch. Req:%s / DB:%s\n", req->UserPW, value.c_str());
                }
            }
            else
            {
                printf("[Redis] User Not Found: %s\n", req->UserID);
                body.Result = static_cast<uint16_t>(ERROR_CODE::LOGIN_USER_NOT_FOUND);
            }

            PushTypedResponse<RedisLoginRes>(RedisTaskID::RESPONSE_LOGIN, task.UserIndex, body);
            break;
        }
        case RedisTaskID::REQUEST_SET_AUTH_TOKEN:
        {
            auto pReq = (RedisAuthTokenReq*)task.pData;

            // SET AuthToken:{Token} {UserIndex} EX 60 (60초 뒤 만료)
            redisReply* reply = (redisReply*)redisCommand(mConn._getCtx(), "SET AuthToken:%s %d EX 60", pReq->Token, pReq->UserIndex);

            if (reply) freeReplyObject(reply);
            break;
        }

        // ------------------------------------------------------------
        case RedisTaskID::REQUEST_NOTICE:
        {
            auto* req = reinterpret_cast<RedisNoticeReq*>(task.pData);
            mConn.publish("ch_notice", req->Message);
            break;
        }

        // ------------------------------------------------------------
        case RedisTaskID::REQUEST_LOAD_INVENTORY:
        {
            auto* req = reinterpret_cast<RedisInvenReq*>(task.pData);
            RedisInvenRes res{};
            res.UserIndex = req->UserIndex;

            std::string key = "u:" + std::string(req->UserID) + ":inven";
            std::map<std::string, std::string> inven;

            if (mConn.hgetall(key, inven) && !inven.empty())
            {
                for (auto& [k, v] : inven)
                {
                    int idx = std::stoi(k);
                    if (idx >= 0 && idx < INVENTORY_SIZE)
                        res.ItemSlots[idx] = std::stoi(v);
                }
            }
            else
            {
                // 신규 유저 - 랜덤 아이템 2개 지급
                AssignNewUserItems(req->UserID, key, res);
            }

            PushTypedResponse<RedisInvenRes>(RedisTaskID::RESPONSE_LOAD_INVENTORY, task.UserIndex, res);
            break;
        }

        // ------------------------------------------------------------
        // 상점 관련 (인게임 미사용, 코드 유지)
        case RedisTaskID::REQUEST_SHOP_UPDATE:
            ProcessShopUpdate(task, isBuying);
            break;

        case RedisTaskID::REQUEST_SHOP_BUY:
            ProcessShopBuy(task, isBuying);
            break;

        default:
            break;
        }
    }

    // ----------------------------------------------------------------
    //  신규 유저 인벤토리 초기화
    // ----------------------------------------------------------------
    void AssignNewUserItems(const char* userID, const std::string& key, RedisInvenRes& res)
    {
        static std::mt19937 gen{ std::random_device{}() };

        int item1 = 101 + (gen() % 5);
        int item2 = 101 + (gen() % 5);

        int nums[INVENTORY_SIZE] = { 0, 1, 2, 3, 4 };

        std::uniform_int_distribution<int> dis1(0, INVENTORY_SIZE - 1);
        int i1 = dis1(gen);
        std::swap(nums[i1], nums[INVENTORY_SIZE - 1]);

        std::uniform_int_distribution<int> dis2(0, INVENTORY_SIZE - 2);
        int i2 = dis2(gen);

        int slot1 = nums[i1];
        int slot2 = nums[i2];

        uint32_t ret;
        for (int i = 0; i < INVENTORY_SIZE; i++)
            mConn.hset(key, std::to_string(i), "0", ret);

        mConn.hset(key, std::to_string(slot1), std::to_string(item1), ret);
        mConn.hset(key, std::to_string(slot2), std::to_string(item2), ret);

        res.ItemSlots[slot1] = item1;
        res.ItemSlots[slot2] = item2;

        printf("[Redis] NewUser(%s) items: slot%d=%d, slot%d=%d\n",
            userID, slot1, item1, slot2, item2);
    }

    // ----------------------------------------------------------------
    //  상점 처리 (인게임 미사용, 코드 유지)
    // ----------------------------------------------------------------
    void ProcessShopUpdate(RedisTask& task, bool& isBuying)
    {
        int commandValue = 0;
        if (task.DataSize == sizeof(int) && task.pData)
            commandValue = *reinterpret_cast<int*>(task.pData);

        time_t now = std::time(nullptr);
        std::string dbTime, dbItem;
        uint64_t storedNextTime = 0;
        int currentItemID = 101;

        mConn.hget("game:shop_state", "next_update_ts", dbTime);
        mConn.hget("game:shop_state", "current_item", dbItem);

        if (!dbTime.empty()) storedNextTime = std::stoull(dbTime);
        if (!dbItem.empty()) currentItemID = std::stoi(dbItem);

        auto GetNextMidnight = [](time_t base) -> uint64_t
            {
                struct tm t;
#if defined(_WIN32)
                localtime_s(&t, &base);
#else
                localtime_r(&base, &t);
#endif
                t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;
                t.tm_mday += 1;
                return static_cast<uint64_t>(mktime(&t));
            };

        bool needUpdate = false;
        uint64_t finalNextTime = storedNextTime;

        static std::mt19937 gen{ std::random_device{}() };

        if (commandValue == 0)
        {
            currentItemID = 101 + (gen() % 5);
            finalNextTime = GetNextMidnight(now);
            needUpdate = true;
            printf("[Shop] Reset\n");
        }
        else if (commandValue > 0)
        {
            uint64_t reduce = static_cast<uint64_t>(commandValue) * 60;
            finalNextTime = (storedNextTime <= static_cast<uint64_t>(now) + reduce)
                ? static_cast<uint64_t>(now) - 1
                : storedNextTime - reduce;
            needUpdate = true;
            printf("[Shop] Fast Forward %d mins\n", commandValue);
        }
        else if (commandValue == -1)
        {
            if (isBuying) { isBuying = false; needUpdate = true; }
            if (storedNextTime == 0 || static_cast<uint64_t>(now) >= storedNextTime)
            {
                currentItemID = 101 + (gen() % 5);
                finalNextTime = GetNextMidnight(now);
                needUpdate = true;
                printf("[Shop] Daily Reset\n");
            }
        }
        else if (commandValue == -2)
        {
            needUpdate = true;
        }

        if (needUpdate)
        {
            uint32_t ret;
            mConn.hset("game:shop_state", "current_item", std::to_string(currentItemID), ret);
            mConn.hset("game:shop_state", "next_update_ts", std::to_string(finalNextTime), ret);
        }

        if (needUpdate || commandValue != -1)
        {
            RedisShopRes res{ currentItemID, finalNextTime };
            PushTypedResponse<RedisShopRes>(RedisTaskID::RESPONSE_SHOP_UPDATE, 0, res);
        }
    }

    void ProcessShopBuy(RedisTask& task, bool& isBuying)
    {
        auto* req = reinterpret_cast<RedisShopBuyReq*>(task.pData);
        RedisShopBuyRes res{};

        std::string key = "u:" + std::string(req->UserID) + ":inven";
        std::map<std::string, std::string> inven;
        mConn.hgetall(key, inven);

        int tempInven[INVENTORY_SIZE] = {};
        for (auto& [k, v] : inven)
        {
            int idx = std::stoi(k);
            if (idx >= 0 && idx < INVENTORY_SIZE)
                tempInven[idx] = std::stoi(v);
        }

        int emptySlot = -1;
        for (int i = 0; i < INVENTORY_SIZE; i++)
        {
            if (tempInven[i] == EMPTYITEM) { emptySlot = i; break; }
        }

        if (emptySlot != -1)
        {
            uint32_t ret;
            mConn.hset(key, std::to_string(emptySlot), std::to_string(req->itemID), ret);
            mConn.hset("game:shop_state", "current_item", "0", ret);
            res.isSuccess = true;
            isBuying = true;
            printf("[Shop] User(%s) Buy Item(%d) at Slot(%d)\n", req->UserID, req->itemID, emptySlot);

            // 구매 성공 시 인벤토리 자동 갱신
            RedisInvenReq invenReq{};
            invenReq.UserIndex = task.UserIndex;
            CopyUserID(invenReq.UserID, req->UserID);
            PushTypedTask<RedisInvenReq>(RedisTaskID::REQUEST_LOAD_INVENTORY, task.UserIndex, invenReq);
        }
        else
        {
            printf("[Shop] User(%s) Buy Failed (Inventory Full)\n", req->UserID);
            res.isSuccess = false;
        }

        PushTypedResponse<RedisShopBuyRes>(RedisTaskID::RESPONSE_SHOP_BUY, task.UserIndex, res);
    }

    // ----------------------------------------------------------------
    //  Subscribe 스레드
    // ----------------------------------------------------------------
    void SubscribeThread()
    {
        printf("RedisManager::SubscribeThread() 시작\n");
        mConnSub.initSubscribe("ch_notice");

        while (mIsTaskRun)
        {
            std::string message;
            mConnSub.subscribe(message);
            if (message.empty()) continue;

            RedisNoticeRes body{};
            CopyUserID(body.UserID, "[GM]");
            strncpy_s(body.Message, sizeof(body.Message), message.c_str(), _TRUNCATE);

            PushTypedResponse<RedisNoticeRes>(RedisTaskID::RESPONSE_NOTICE, 0 /*전체*/, body);
        }

        printf("RedisManager::SubscribeThread() 종료\n");
    }

    // ----------------------------------------------------------------
    //  헬퍼: 타입 안전 Response / Task 생성
    // ----------------------------------------------------------------
    template<typename T>
    void PushTypedResponse(RedisTaskID id, int userIndex, const T& data)
    {
        RedisTask res;
        res.TaskID = id;
        res.UserIndex = userIndex;
        res.DataSize = sizeof(T);
        res.pData = new char[res.DataSize];
        memcpy(res.pData, &data, res.DataSize);
        PushResponse(res);
    }

    template<typename T>
    void PushTypedTask(RedisTaskID id, int userIndex, const T& data)
    {
        RedisTask t;
        t.TaskID = id;
        t.UserIndex = userIndex;
        t.DataSize = sizeof(T);
        t.pData = new char[t.DataSize];
        memcpy(t.pData, &data, t.DataSize);
        PushTask(t);
    }

    // ----------------------------------------------------------------
    //  멤버 변수
    // ----------------------------------------------------------------
    RedisCpp::CRedisConnEx mConn;
    RedisCpp::CRedisConnEx mConnSub;

    bool                     mIsTaskRun = false;
    std::vector<std::thread> mTaskThreads;

    std::mutex            mReqLock;
    std::deque<RedisTask> mRequestTask;

    std::mutex            mResLock;
    std::deque<RedisTask> mResponseTask;
};