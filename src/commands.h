#ifndef COMMANDS_H
#define COMMANDS_H

#include "resp.h"

#include <string>
#include <unordered_map>
#include <functional>

class CommandExecutor {
public:
    using CommandFunc = std::function<Resp(const Resp& cmd)>;
    CommandExecutor();
    Resp execute(const Resp& cmd) const noexcept;
private:
    std::unordered_map<std::string, CommandFunc> commandMap;
    std::unordered_map<std::string, std::string> storage;
    
    Resp handlePing(const Resp& cmd) noexcept;
    Resp handleEcho(const Resp& cmd) noexcept;
    Resp handleGet(const Resp& cmd) noexcept;
    Resp handleSet(const Resp& cmd) noexcept;
};

#endif 