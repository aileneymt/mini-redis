#include "commands.h"

#include <stdexcept>

CommandExecutor::CommandExecutor() {
    commandMap["ECHO"] = [this](const Resp& cmd) { return handleEcho(cmd); };
    commandMap["PING"] = [this](const Resp& cmd) { return handlePing(cmd); };
    commandMap["GET"] = [this](const Resp& cmd) { return handleGet(cmd); };
    commandMap["SET"] = [this](const Resp& cmd) { return handleSet(cmd); };
    commandMap["RPUSH"] = [this](const Resp& cmd) { return handleRpush(cmd); };

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
    
    if (it->second.isExpired()) {
        storage.erase(it); 
        return Resp::nullBulkString();
    }

    return Resp::bulkString(it->second.asString());
}

Resp CommandExecutor::handleSet(const Resp& cmd) noexcept {
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

Resp CommandExecutor::handleRpush(const Resp& cmd) noexcept {
    // append elements to a list
    // if the list doesn't exist, it is created
    // returns a RESP integer w number of elements in list after appending

    const RespVec& args = cmd.asArray();
    if (args.size() < 2) return Resp::error("ERR invalid number of arguments for RPUSH");
    const std::string& list_key = args[1].asString();
    const std::string& list_val = args[2].asString();
    
    auto it = storage.find(list_key);
    if (it == storage.end()) {
        storage[list_key] = {std::vector<std::string>{list_val}};
        return Resp::integer(1);
    }
    else if (!it->second.isList()) {
        return Resp::error("ERR " + list_key + " is not a list");
    }
    else {
        return Resp::error("ERR not yet implemented");
    }


}