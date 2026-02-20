#include "commands.h"

#include <stdexcept>
#include <iostream>

CommandExecutor::CommandExecutor() {
    commandMap["ECHO"] = [this](const Resp& cmd) { return handle_echo(cmd); };
    commandMap["PING"] = [this](const Resp& cmd) { return handle_ping(cmd); };
    commandMap["GET"] = [this](const Resp& cmd) { return handle_get(cmd); };
    commandMap["SET"] = [this](const Resp& cmd) { return handle_set(cmd); };
    commandMap["RPUSH"] = [this](const Resp& cmd) { return handle_push(cmd); };
    commandMap["LPUSH"] = [this](const Resp& cmd) { return handle_push(cmd, false); };
    commandMap["LRANGE"] = [this](const Resp& cmd) { return handle_lrange(cmd); };
    commandMap["LLEN"] = [this](const Resp& cmd) { return handle_llen(cmd); };
    commandMap["LPOP"] = [this](const Resp& cmd) { return handle_lpop(cmd); };
    commandMap["BLPOP"] = [this](const Resp& cmd) { return handle_blpop(cmd); };
}

Resp CommandExecutor::execute(const Resp& cmd) const noexcept {
    if (cmd.type != RespType::Array)
        return Resp::error("ERR invalid RESP type, expected non-empty array");
    
    const RespVec& bulk_str_arr = cmd.asArray(); // ref to avoid copying, since asArray() copies
    if (bulk_str_arr.empty())
        return Resp::error("ERR invalid RESP type, expected non-empty array");
    
    std::string cmd_str = bulk_str_arr[0].asString();
    make_upper(cmd_str);

    auto it = commandMap.find(cmd_str);
    if (it == commandMap.end())
        return Resp::error("ERR invalid command '" + cmd_str + "'");

    return it->second(cmd);
}

Resp CommandExecutor::handle_ping(const Resp& cmd) noexcept {
    return Resp::simpleString("PONG");
}

Resp CommandExecutor::handle_echo(const Resp& cmd) noexcept {
    const auto& args = cmd.asArray();
    if (args.size() < 2)
        return Resp::error("ERR invalid number of arguments for 'echo'");
    
    std::string response = args[1].asString();
    for (size_t i{2}; i < args.size(); ++i) {
        try {
            response.push_back(' ');
            response += args[i].asString();
        } catch (...) {
            return Resp::error("ERR invalid argument type");
        }
    }
    return Resp::bulkString(response);
}

Resp CommandExecutor::handle_get(const Resp& cmd) noexcept {
    const RespVec& args = cmd.asArray();
    if (args.size() < 2)
        return Resp::error("ERR invalid number of arguments for 'get'");

    auto it = storage.find(args[1].asString());
    if (it == storage.end())
        return Resp::nullBulkString();
    
    if (it->second.isExpired()) {
        storage.erase(it); 
        return Resp::nullBulkString();
    }

    return Resp::bulkString(it->second.asString());
}

Resp CommandExecutor::handle_set(const Resp& cmd) noexcept {
    const RespVec& args = cmd.asArray();
    if (args.size() < 3)
        return Resp::error("ERR invalid number of arguments for 'get'");
    
    const std::string& key = args[1].asString();
    const std::string& val = args[2].asString();
    
    StorageEntry entry {val};
    
    for (size_t i = 3; i < args.size(); i += 2) {
        std::string option = args[i].asString();
        make_upper(option);
        if (i + 1 >= args.size()) return Resp::error("ERR syntax error");

        int time = std::stoi(args[i + 1].asString());
        if (option == "EX") {
            entry.expiry = std::chrono::steady_clock::now() + 
                          std::chrono::seconds(time);
        }
        else if (option == "PX") {
            entry.expiry = std::chrono::steady_clock::now() + 
                          std::chrono::milliseconds(time);
        }
        else {
            return Resp::error("ERR unimplemented");
        }
    }
    storage[key] = entry;
    return Resp::simpleString("OK");
}

Resp CommandExecutor::handle_push(const Resp& cmd, const bool rPush) noexcept {
    const RespVec& args = cmd.asArray();
    if (args.size() < 2) return Resp::error("ERR invalid number of arguments for RPUSH");
    const std::string& list_key = args[1].asString();
    
    std::unique_lock<std::mutex> storage_lock(storage_mutex);
    auto it = storage.find(list_key);
    if (it != storage.end() && !it->second.isList())
        return Resp::error("ERR " + list_key + " exists and is not a list");

    if (it == storage.end()) {
        storage[list_key] = StorageEntry{StringList()};
        it = storage.find(list_key);
    }
    
    auto& list_vals = it->second.asList();
    for (size_t i {2}; i < args.size(); ++i)
        push_string(list_vals, args[i].asString(), rPush);
    int size = list_vals.size();
    storage_lock.unlock();
    {
        std::lock_guard<std::mutex> key_cvs_lock(key_cvs_mutex);
        auto cv_it = key_cvs.find(list_key);
        if (cv_it != key_cvs.end())
            cv_it->second->notify_all();
    }
    return Resp::integer(size);
}

Resp CommandExecutor::handle_lrange(const Resp& cmd) noexcept {
    const RespVec& args = cmd.asArray();
    if (args.size() != 4) return Resp::error("ERR invalid number of arguments for LRANGE, expected 2 indices");
   
    const std::string& list_key = args[1].asString();
    
    auto start_idx_opt = parse_int(args[2]);
    auto end_idx_opt = parse_int(args[3]);
    
    if (!start_idx_opt || !end_idx_opt) return Resp::error("ERR start and end indices must be integers");
    int start_idx = *start_idx_opt;
    int end_idx = *end_idx_opt;

    auto it = storage.find(list_key);
    if (it == storage.end()) return Resp::array({});
    if (!it->second.isList()) return Resp::error("ERR " + list_key + " is not a list");
    
    auto& list = it->second.asList(); // std::vector<std::string>
    int size = list.size();
    start_idx = normalize_index(start_idx, size);
    end_idx = std::min(normalize_index(end_idx, size) + 1, size);

    std::vector<Resp> splice;
    for (int i{start_idx}; i < end_idx; ++i) {
        splice.emplace_back(Resp::bulkString(list[i]));
    }
    return Resp::array(splice);
    
}

Resp CommandExecutor::handle_llen(const Resp& cmd) noexcept {
    const RespVec& args = cmd.asArray();
    if (args.size() != 2) return Resp::error("ERR invalid number of arguments for LLEN");
    const std::string& list_key = args[1].asString();
    
    auto it = storage.find(list_key);
    if (it == storage.end()) return Resp::integer(0);
    if (!it->second.isList()) return Resp::error("ERR " + list_key + " is not a list");
    return Resp::integer(it->second.asList().size());
}

Resp CommandExecutor::handle_lpop(const Resp& cmd) noexcept {
    const RespVec& args = cmd.asArray();
    if (args.size() < 2 || args.size() > 3) return Resp::error("ERR invalid number of arguments for LPOP");
    
    const std::string& list_key = args[1].asString();
    int count = 1;
    if (args.size() == 3) {
        std::optional<int> count_opt = parse_int(args[2]);
        if (!count_opt.has_value()) return Resp::error("ERR expected valid integer for the number of elements to remove");
        count = *count_opt;
    }

    auto it = storage.find(list_key);
    if (it == storage.end()) return Resp::nullBulkString();
    if (!it->second.isList()) return Resp::error("ERR " + list_key + " is not a list");
    
    auto& list = it->second.asList();
    if (list.empty()) return Resp::nullBulkString();
    RespVec popped;
    popped.reserve(count);
    while (!list.empty() && count) {
        popped.emplace_back(Resp::bulkString(std::move(list[0])));
        list.pop_front();
        --count;
    }
    if (popped.size() == 1) return std::move(popped[0]);
    return Resp::array(popped);
}

Resp CommandExecutor::handle_blpop(const Resp& cmd) noexcept {
    std::unique_lock<std::mutex> storage_lock(storage_mutex);

    const RespVec& args = cmd.asArray();
    if (args.size() < 2) return Resp::error("ERR invalid number of arguments for BLPOP");

    const std::string& list_key = args[1].asString();
    int timeout = 0; // in ms
    if (args.size() > 2) {
        timeout = std::stof(args[2].asString()) * 1000;
        std::cout << "TIMEOUT: " << timeout << std::endl;
    }

    auto it = storage.find(list_key);
    if (it != storage.end() && !it->second.isList()) return Resp::error("ERR " + list_key + " is not a list");
    if (it == storage.end() || it->second.asList().empty()) {
        std::shared_ptr<std::condition_variable> cv;
        {
            std::lock_guard<std::mutex> key_cvs_lock(key_cvs_mutex);
            if (key_cvs.find(list_key) == key_cvs.end()) {
                key_cvs[list_key] = std::make_shared<std::condition_variable>();
            }
            cv = key_cvs[list_key];
        }
        auto checkListForItems {
            [&]{ 
                it = storage.find(list_key);
                return it != storage.end() && !it->second.asList().empty();
            }
        };
        if (timeout == 0) {  // no timeout
            cv->wait(storage_lock, checkListForItems);
        } else {
            if (!cv->wait_for(storage_lock, std::chrono::milliseconds(timeout), checkListForItems)) 
            { 
                return Resp::nullArray();
            }  
        }
    }
    auto& list = it->second.asList();
    const auto popped_str = std::move(list[0]);
    list.pop_front();
    return Resp::array({Resp::bulkString(list_key), Resp::bulkString(std::move(popped_str))});
}

std::optional<int> CommandExecutor::parse_int(const Resp& arg) noexcept {
    try {
        int i = std::stoi(arg.asString());
        return i;
    } catch (...) {
        return std::nullopt;
    }
}

/**
 * Converts a negative index to it's corresponding positive index.
 * If the index is already positive, returns with no change.
 * If the index is OOB for the size (such as -5 for an array of size 3), returns 0
 * Will only ever return a non-negative value
 */
int CommandExecutor::normalize_index(int i, const int size) noexcept {
    if (i < 0) i += size;
    return std::clamp(i, 0, size);
}


void CommandExecutor::push_string(StringList& list, std::string str, const bool rPush) {
    if (rPush)
        list.push_back(std::move(str));
    else
        list.push_front(std::move(str));
    
}