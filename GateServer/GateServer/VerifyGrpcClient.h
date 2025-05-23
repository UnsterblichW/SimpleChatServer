#ifndef _VerifyGRPCCLIENT_H_
#define _VerifyGRPCCLIENT_H_

#include "const.h"
#include "Singleton.h"
#include "ConfigMgr.h"
#include "message.grpc.pb.h"
#include <grpcpp/grpcpp.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;


class RPConPool {
public:
    RPConPool(size_t poolSize, std::string host, std::string port)
        : b_stop_(false),
          poolSize_(poolSize),
          host_(host),
          port_(port) {
        for (size_t i = 0; i < poolSize_; ++i) {
            std::shared_ptr<Channel> channel = grpc::CreateChannel(
                host + ":" + port, grpc::InsecureChannelCredentials());

            connections_.push(VerifyService::NewStub(channel));
        }
    }

    ~RPConPool() {
        std::lock_guard<std::mutex> lock(mutex_);
        Close();
        while (!connections_.empty()) {
            connections_.pop();
        }
    }

    std::unique_ptr<VerifyService::Stub> getConnection() {
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
        auto context = std::move(connections_.front());
        connections_.pop();
        return context;
    }

    void recycleConnection(std::unique_ptr<VerifyService::Stub> context) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (b_stop_) {
            return;
        }
        connections_.push(std::move(context));
        cond_.notify_one();
    }

    void Close() {
        b_stop_ = true;
        cond_.notify_all();
    }

private:
    atomic<bool> b_stop_;
    size_t poolSize_;
    std::string host_;
    std::string port_;
    std::queue<std::unique_ptr<VerifyService::Stub>> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

class VerifyGrpcClient : public Singleton<VerifyGrpcClient> {
    friend class Singleton<VerifyGrpcClient>;

public:
    GetVerifyRsp GetVerifyCode(std::string email) {
        ClientContext context;
        GetVerifyRsp reply;
        GetVerifyReq request;
        request.set_email(email);

        auto stub = pool_->getConnection();
        Status status = stub->GetVerifyCode(&context, request, &reply);
        if (status.ok()) {
            pool_->recycleConnection(std::move(stub));
            return reply;
        } else {
            pool_->recycleConnection(std::move(stub));
            reply.set_error(ErrorCodes::RPCFailed);
            return reply;
        }
    }

private:
    VerifyGrpcClient() {
        auto& gCfgMgr = ConfigMgr::Inst();
        std::string host = gCfgMgr["VerifyServer"]["Host"];
        std::string port = gCfgMgr["VerifyServer"]["Port"];
        pool_.reset(new RPConPool(5, host, port));
    }

    std::unique_ptr<RPConPool> pool_;
};

#endif
