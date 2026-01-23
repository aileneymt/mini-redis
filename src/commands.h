#ifndef COMMANDS_H
#define COMMANDS_H

#include "resp.h"

#include <string>
#include <unordered_map>
#include <functional>

class CommandExecutor {
public:
    using CommandFunc = std::function<Resp(const Resp& cmd)>;
    std::unordered_map<std::string, CommandFunc> commandMap;

    CommandExecutor();
    Resp execute(const Resp& cmd) const noexcept;
private:
    static Resp handlePing(const Resp& cmd) noexcept;
    static Resp handleEcho(const Resp& cmd) noexcept;
};



#endif 