#pragma once

#include "../thirdparty/CRedisConn.h"
//#include "../thirdparty/hiredis/async.h"


namespace RedisCpp
{
    class CRedisConnEx: public CRedisConn
    {
    public:
        void publish(const std::string& channel, const std::string& message)
		{
			printf("[Redis Publish] (%s) : %s\n", channel.c_str(), message.c_str());
			// #1 PUBLISH channel message
			redisReply* reply = static_cast<redisReply*>(redisCommand(_getCtx(), "PUBLISH %s %s", channel.c_str(), message.c_str()));
			if (reply == NULL) {
				printf("[Redis Publish] redisCommand reply is NULL: %s\n", _getCtx()->errstr);
				return;
			}
			if (reply->type == REDIS_REPLY_ERROR) {
				printf("[Redis Publish] Command Error: %s\n", reply->str);
				freeReplyObject(reply);
				return;
			}

			freeReplyObject(reply);
		}

        bool initSubscribe(const std::string& channel)
		{
			printf("[Redis initSubscribe] (%s)\n", channel.c_str());

			// #1 SUBSCRIBE channel 
			redisReply* reply = static_cast<redisReply*>(redisCommand(_getCtx(), "SUBSCRIBE %s", channel.c_str()));
			if (reply == NULL) {
				printf("[Redis initSubscribe] redisCommand reply is NULL: %s\n", _getCtx()->errstr);
				return false;
			}
			if (reply->type == REDIS_REPLY_ERROR) {
				printf("[Redis initSubscribe] Command Error: %s\n", reply->str);
				freeReplyObject(reply);
				return false;
			}
		}

		void subscribe(std::string& message/*output*/)
		{
			redisReply* reply = NULL;
			freeReplyObject(reply);

			if (redisGetReply(_getCtx(), (void**)&reply) ==  REDIS_OK)
			{
				std::string channelName;
				// 0: "message"
				// 1: [channel name]
				// 2: [message string]
				for (int i = 0; i < reply->elements; i++) {
					if (i == 1)
					{
						channelName = reply->element[i]->str;
					}
					else if (i == 2)
					{
						message = reply->element[i]->str;
						printf("[Redis Subscribe] (%s) : %s\n", channelName.c_str(), message.c_str());
					}
				}
			}

			freeReplyObject(reply);
		}
    };


}

