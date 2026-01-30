#ifndef COMMANDS_H
#define COMMANDS_H

#include "resp.h"

#include <string>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <optional>
#include <variant>

struct StorageEntry {
    std::variant<std::string, std::vector<std::string>> value;
    std::optional<std::chrono::time_point<std::chrono::steady_clock>> expiry;

    bool isString() {
        return std::holds_alternative<std::string>(value);
    }
    bool isList() {
        return std::holds_alternative<std::vector<std::string>>(value);
    }

    bool isExpired() {
        return expiry.has_value() && std::chrono::steady_clock::now() > *expiry;
    }

    std::string& asString() {
        if (!isString()) throw std::runtime_error("value type is not std::string");
        return std::get<std::string>(value);
    }
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
    Resp handleRpush(const Resp& cmd) noexcept;

};

#endif 