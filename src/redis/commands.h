#ifndef COMMANDS_H
#define COMMANDS_H

#include "../resp/resp.h"
#include "storage.h"

#include <string>
#include <unordered_map>
#include <functional>

#include <mutex>
#include <condition_variable>

class CommandExecutor {
public:
    using CommandFunc = std::function<Resp(const Resp& cmd)>;
    CommandExecutor();
    Resp execute(const Resp& cmd) const noexcept;
    static void make_upper(std::string& str) {
        std::transform(str.begin(), str.end(), str.begin(),
            [](unsigned char c){ return std::toupper(c); }); // Use a lambda for safety/clarity
    }
private:
    std::unordered_map<std::string, CommandFunc> commandMap;
    std::unordered_map<std::string, StorageEntry> storage;
    std::unordered_map<std::string, std::shared_ptr<std::condition_variable>> key_cvs;
    std::mutex key_cvs_mutex;  // protects the above map
    std::mutex storage_mutex; // protects storage

    Resp handle_ping(const Resp& cmd) noexcept;
    Resp handle_echo(const Resp& cmd) noexcept;
    Resp handle_get(const Resp& cmd) noexcept;
    Resp handle_set(const Resp& cmd) noexcept;
    Resp handle_push(const Resp& cmd, const bool rPush=true) noexcept;
    Resp handle_lrange(const Resp& cmd) noexcept;
    Resp handle_llen(const Resp& cmd) noexcept;
    Resp handle_lpop(const Resp& cmd) noexcept;
    Resp handle_blpop(const Resp& cmd) noexcept;
    Resp handle_type(const Resp& cmd) noexcept;

    static std::optional<int> parse_int(const Resp& arg) noexcept;
    static int normalize_index(int i, const int size) noexcept;
    static void push_string(StringList& list, std::string str, const bool rPush);

};

#endif 