#include "commands.h"

#include <stdexcept>
#include <algorithm>

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
}

Resp CommandExecutor::execute(const Resp& cmd) const noexcept {
    if (cmd.type != RespType::Array)
        return Resp::error("ERR invalid RESP type, expected non-empty array");
    
    const RespVec& bulk_str_arr = cmd.asArray(); // ref to avoid copying, since asArray() copies
    if (bulk_str_arr.empty())
        return Resp::error("ERR invalid RESP type, expected non-empty array");
    
    std::string cmd_str = bulk_str_arr[0].asString();

    std::transform(cmd_str.begin(), cmd_str.end(), cmd_str.begin(),
        [](unsigned char c){ return std::toupper(c); }); // Use a lambda for safety/clarity

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
        std::transform(option.begin(), option.end(), option.begin(), ::toupper);
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
    
    auto it = storage.find(list_key);
    if (it == storage.end()) {
        StringList list_vals;
        for (size_t i {2}; i < args.size(); ++i)
            push_string(list_vals, args[i].asString(), rPush);
        storage[list_key] = {list_vals};
        return Resp::integer(list_vals.size());
    }
    else if (!it->second.isList()) {
        return Resp::error("ERR " + list_key + " exists and is not a list");
    }
    
    auto& list_vals = it->second.asArray();
    for (size_t i {2}; i < args.size(); ++i)
        push_string(list_vals, args[i].asString(), rPush);
    return Resp::integer(list_vals.size());
}

Resp CommandExecutor::handle_lrange(const Resp& cmd) noexcept {
    const RespVec& args = cmd.asArray();
    if (args.size() != 4) return Resp::error("ERR invalid number of arguments for LRANGE, expected 2 indices");
   
    const std::string& list_key = args[1].asString();
    
    auto start_idx_opt = parseIndex(args[2]);
    auto end_idx_opt = parseIndex(args[3]);
    
    if (!start_idx_opt || !end_idx_opt) return Resp::error("ERR start and end indices must be integers");
    int start_idx = *start_idx_opt;
    int end_idx = *end_idx_opt;

    auto it = storage.find(list_key);
    if (it == storage.end()) return Resp::array({});
    if (!it->second.isList()) return Resp::error("ERR " + list_key + " is not a list");
    
    auto& list = it->second.asArray(); // std::vector<std::string>
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
    return Resp::integer(it->second.asArray().size());
}



std::optional<int> CommandExecutor::parseIndex(const Resp& arg) noexcept {
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