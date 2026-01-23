#include "resp.h"
#include <cctype>

/* ----------------------------- Resp FUNCTIONS --------------------------*/
Resp Resp::simpleString(const std::string& s) {
    Resp r;
    r.value = s;
    r.type = RespType::SimpleString;
    return r;
}
Resp Resp::error(const std::string& s) {
    Resp r;
    r.value = s;
    r.type = RespType::Error;
    return r;
}
Resp Resp::integer(const int i) {
    Resp r;
    r.value = i;
    r.type = RespType::Integer;
    return r;
}
Resp Resp::bulkString(const std::string& s) {
    Resp r;
    r.value = s;
    r.type = RespType::BulkString;
    return r;
}
Resp Resp::array(const std::vector<Resp>& arr) {
    Resp r;
    r.value = arr;
    r.type = RespType::Array;
    return r;
}

std::string Resp::encode() const {

}

/* ------------------------- RespParser functions ------------ */
bool RespParser::expectCRLF() {
    if (pos + 1 >= data.size() || data[pos] != '\r' || data[pos + 1] != '\n') {
        return false;
    }
    pos += 2;
    return true;
}

std::optional<int> RespParser::readInt(bool posOk) {
    bool isNeg = false;
    if (data[pos] == '+' || data[pos] == '-') {
        if (!posOk && data[pos] == '+') return std::nullopt;
        isNeg = data[pos] == '-';
        ++pos;
    }

    int num {0};
    while (pos < data.size() && std::isdigit(data[pos])) {
        num = num * 10 + data[pos++] - '0';
    }
    if (!expectCRLF()) return std::nullopt;
    return isNeg ? num * -1 : num;
}

std::optional<Resp> RespParser::parseInt() {
    ++pos;
    auto result = readInt();
    if (!result) return std::nullopt;
    return Resp::integer(*result);
}

std::optional<Resp> RespParser::parseError() {
    if (++pos >= data.size()) return std::nullopt;

    std::string err{};
    while (pos < data.size() && std::isalnum(data[pos])) {
        err.push_back(data[pos++]);
    }
    if (!expectCRLF()) return std::nullopt;
    return Resp::error(err);
}

std::optional<Resp> RespParser::parseBulkString() {
    // Invalid if empty or the first byte isn't '-' or '1'
    if (++pos >= data.size()) return std::nullopt;
    
    auto len = readInt(false);
    if (!len) return std::nullopt;
    if (*len == -1) return Resp::nullBulkString();

    // Process the actual string
    std::string str{};
    while (pos < data.size() && data[pos] != '\r')
        str.push_back(data[pos++]);

    if (str.length() != *len || !expectCRLF()) return std::nullopt;

    return Resp::bulkString(str);
}

std::optional<Resp> RespParser::parseSimpleString() {
    if (++pos < data.size()) return std::nullopt;
    std::string str{};
    while (pos < data.size() && std::isalnum(data[pos])) {
        str.push_back(data[pos++]);
    }

    if (!expectCRLF()) return std::nullopt;
    return Resp::simpleString(str);
}

std::optional<Resp> RespParser::parseArray() {
    if (++pos < data.size()) return std::nullopt;
    auto len = readInt(false);
    if (!len || *len < -1) return std::nullopt;
    
    std::vector<Resp> arr{};
    
    for (int i {0}; i < *len; ++i) {
        if (pos >= data.size()) return std::nullopt;
        std::optional<Resp> r;
        switch(data[pos]) {
            case '+':
                r = parseSimpleString();
                break;
            case '-':
                r = parseError();
                break;
            case ':':
                r = parseInt();
                break;
            case '$':
                r = parseBulkString();
                break;
            case '*':
                r = parseArray();
                break;
            default:
                return std::nullopt; // invalid type
        }
        if (!r) return std::nullopt;
        arr.push_back(*r);
    }

    if (*len > -1 && *len != arr.size()) return std::nullopt;
    return Resp::array(arr);

}

std::optional<Resp> RespParser::parse() {
    if (data.empty()) return std::nullopt;

    std::optional<Resp> r;
    switch(data[0]) {
        case '+':
            r = parseSimpleString();
            break;
        case '-':
            r = parseError();
            break;
        case ':':
            r = parseInt();
            break;
        case '$':
            r = parseBulkString();
            break;
        case '*':
            r = parseArray();
            break;
        default:
            return std::nullopt; // invalid type
    }
    if (pos != data.size()) return std::nullopt;
    return r;
}