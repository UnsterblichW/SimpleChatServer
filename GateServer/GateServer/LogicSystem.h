#ifndef _LOGICSYSTEM_H_
#define _LOGICSYSTEM_H_

#include "Singleton.h"
#include <functional>
#include <map>

class HttpConnection;
typedef std::function<void(std::shared_ptr<HttpConnection>)> HttpHandler;

class LogicSystem : public Singleton<LogicSystem> {
    friend class Singleton<LogicSystem>;

public:
    ~LogicSystem();
    bool HandleGet(std::string, std::shared_ptr<HttpConnection>);
    void RegGet(std::string, HttpHandler handler);
    void RegPost(std::string, HttpHandler handler);
    bool HandlePost(std::string, std::shared_ptr<HttpConnection>);

private:
    LogicSystem();
    std::map<std::string, HttpHandler> _post_handlers;
    std::map<std::string, HttpHandler> _get_handlers;
};

#endif
