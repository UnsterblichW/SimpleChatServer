#ifndef _REDISMGR_H_
#define _REDISMGR_H_

#include "Singleton.h"
#include <atomic>
#include <hiredis/hiredis.h>
#include <mutex>
#include <queue>

class RedisConPool {
public:
    RedisConPool(size_t poolSize, char const *host, int port, char const *pwd)
        : poolSize_(poolSize),
          host_(host),
          port_(port),
          b_stop_(false),
          pwd_(pwd),
          counter_(0) {
        for (size_t i = 0; i < poolSize_; ++i) {
            auto *context = redisConnect(host, port);
            if (context == nullptr || context->err != 0) {
                if (context != nullptr) {
                    redisFree(context);
                }
                continue;
            }

            auto reply = (redisReply *)redisCommand(context, "AUTH %s", pwd);
            if (reply->type == REDIS_REPLY_ERROR) {
                std::cout << "认证失败" << std::endl;
                // 执行成功 释放redisCommand执行后返回的redisReply所占用的内存
                freeReplyObject(reply);
                continue;
            }

            // 执行成功 释放redisCommand执行后返回的redisReply所占用的内存
            freeReplyObject(reply);
            std::cout << "认证成功" << std::endl;
            connections_.push(context);
        }

        check_thread_ = std::thread([this]() {
            while (!b_stop_) {
                counter_++;
                if (counter_ >= 60) {
                    checkThread();
                    counter_ = 0;
                }

                std::this_thread::sleep_for(
                    std::chrono::seconds(1)); // 每隔 30 秒发送一次 PING 命令
            }
        });
    }

    ~RedisConPool() {}

    void ClearConnections() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!connections_.empty()) {
            auto *context = connections_.front();
            redisFree(context);
            connections_.pop();
        }
    }

    redisContext *getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] {
            if (b_stop_) {
                return true;
            }
            return !connections_.empty();
        });
        // 如果停止则直接返回空指针
        if (b_stop_) {
            return nullptr;
        }
        auto *context = connections_.front();
        connections_.pop();
        return context;
    }

    void returnConnection(redisContext *context) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (b_stop_) {
            return;
        }
        connections_.push(context);
        cond_.notify_one();
    }

    void Close() {
        b_stop_ = true;
        cond_.notify_all();
        check_thread_.join();
    }

private:
    void checkThread() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (b_stop_) {
            return;
        }
        auto pool_size = connections_.size();
        for (int i = 0; i < pool_size && !b_stop_; i++) {
            auto *context = connections_.front();
            connections_.pop();
            try {
                auto reply = (redisReply *)redisCommand(context, "PING");
                if (!reply) {
                    std::cout << "redis ping failed" << std::endl;
                    connections_.push(context);
                    continue;
                }
                freeReplyObject(reply);
                connections_.push(context);
            } catch (std::exception &exp) {
                std::cout << "Error keeping connection alive: " << exp.what()
                          << std::endl;
                redisFree(context);
                context = redisConnect(host_, port_);
                if (context == nullptr || context->err != 0) {
                    if (context != nullptr) {
                        redisFree(context);
                    }
                    continue;
                }

                auto reply =
                    (redisReply *)redisCommand(context, "AUTH %s", pwd_);
                if (reply->type == REDIS_REPLY_ERROR) {
                    std::cout << "认证失败" << std::endl;
                    // 执行成功
                    // 释放redisCommand执行后返回的redisReply所占用的内存
                    freeReplyObject(reply);
                    continue;
                }

                // 执行成功 释放redisCommand执行后返回的redisReply所占用的内存
                freeReplyObject(reply);
                std::cout << "认证成功" << std::endl;
                connections_.push(context);
            }
        }
    }

    std::atomic<bool> b_stop_;
    size_t poolSize_;
    char const *host_;
    char const *pwd_;
    int port_;
    std::queue<redisContext *> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::thread check_thread_;
    int counter_;
};

class RedisMgr : public Singleton<RedisMgr>,
                 public std::enable_shared_from_this<RedisMgr> {
    friend class Singleton<RedisMgr>;

public:
    ~RedisMgr();
    bool Get(std::string const &key, std::string &value);
    bool Set(std::string const &key, std::string const &value);
    bool LPush(std::string const &key, std::string const &value);
    bool LPop(std::string const &key, std::string &value);
    bool RPush(std::string const &key, std::string const &value);
    bool RPop(std::string const &key, std::string &value);
    bool HSet(std::string const &key, std::string const &hkey,
              std::string const &value);
    bool HSet(char const *key, char const *hkey, char const *hvalue,
              size_t hvaluelen);
    std::string HGet(std::string const &key, std::string const &hkey);
    bool HDel(std::string const &key, std::string const &field);
    bool Del(std::string const &key);
    bool ExistsKey(std::string const &key);

    void Close() {
        _con_pool->Close();
        _con_pool->ClearConnections();
    }

private:
    RedisMgr();
    unique_ptr<RedisConPool> _con_pool;
};

#endif
