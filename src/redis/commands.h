#ifndef COMMANDS_H
#define COMMANDS_H

#include "../resp/resp.h"
#include "storage.h"

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
    std::unordered_map<std::string, StorageEntry> storage;
    
    Resp handlePing(const Resp& cmd) noexcept;
    Resp handleEcho(const Resp& cmd) noexcept;
    Resp handleGet(const Resp& cmd) noexcept;
    Resp handleSet(const Resp& cmd) noexcept;
    Resp handleRpush(const Resp& cmd) noexcept;
    Resp handleLrange(const Resp& cmd) noexcept;

    static std::optional<int> parseIndex(const Resp& arg) noexcept;
    static int normalize_index(int i, const int size) noexcept;
};

#endif 