#ifndef COMMANDS_H
#define COMMANDS_H

#include "resp.h"

#include <string>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <optional>

struct StorageEntry {
    std::string val;
    std::optional<std::chrono::time_point<std::chrono::steady_clock>> expiry;
};

class CommandExecutor {
public:
    using CommandFunc = std::function<Resp(const Resp& cmd)>;
    CommandExecutor();
    Resp execute(const Resp& cmd) const noexcept;
private:
    std::unordered_map<std::string, CommandFunc> commandMap;
    std::unordered_map<std::string, StorageEntry> storage;
    
    Resp handlePing(const Resp& cmd) noexcept;
    Resp handleEcho(const Resp& cmd) noexcept;
    Resp handleGet(const Resp& cmd) noexcept;
    Resp handleSet(const Resp& cmd) noexcept;
};

#endif 