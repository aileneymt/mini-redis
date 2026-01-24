#include "commands.h"
#include <algorithm>
#include <cctype>

CommandExecutor::CommandExecutor() {
    commandMap["ECHO"] = [this](const Resp& cmd) { return handleEcho(cmd); };
    commandMap["PING"] = [this](const Resp& cmd) { return handlePing(cmd); };
    commandMap["GET"] = [this](const Resp& cmd) { return handleGet(cmd); };
    commandMap["SET"] = [this](const Resp& cmd) { return handleSet(cmd); };
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

Resp CommandExecutor::handlePing(const Resp& cmd) noexcept {
    return Resp::simpleString("PONG");
}

Resp CommandExecutor::handleEcho(const Resp& cmd) noexcept {
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

Resp CommandExecutor::handleGet(const Resp& cmd) noexcept {
    const RespVec& args = cmd.asArray();
    if (args.size() < 2)
        return Resp::error("ERR invalid number of arguments for 'get'");

    auto it = storage.find(args[1].asString());
    if (it == storage.end())
        return Resp::nullBulkString();
    
    if (it->second.expiry.has_value() &&
        std::chrono::steady_clock::now() > *it->second.expiry) {
        storage.erase(it); 
        return Resp::nullBulkString();
    }

    return Resp::bulkString(it->second.val);
}

Resp CommandExecutor::handleSet(const Resp& cmd) noexcept {
    const RespVec& args = cmd.asArray();
    if (args.size() < 3)
        return Resp::error("ERR invalid number of arguments for 'get'");
    
    const std::string& key = args[1].asString();
    const std::string& val = args[2].asString();
    
    StorageEntry entry{val};
    
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