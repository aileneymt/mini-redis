#include "commands.h"
#include <algorithm>
#include <cctype>

CommandExecutor::CommandExecutor() {
    commandMap["ECHO"] = handleEcho;
    commandMap["PING"] = handlePing;
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
    const auto& og = cmd.asArray();
    if (og.size() < 2)
        return Resp::error("ERR invalid number of arguments for 'echo'");
    
    std::string response;
    for (int i{1}; i < og.size(); ++i) {
        try {
            if (i > 1) response.push_back(' ');
            response += og[i].asString();
        } catch (...) {
            return Resp::error("ERR invalid argument type");
        }
    }
    return Resp::bulkString(response);
}