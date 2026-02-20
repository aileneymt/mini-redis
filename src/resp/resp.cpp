#include "resp.h"
#include <cctype>
#include <stdexcept>

/* ----------------------------- Resp FUNCTIONS --------------------------*/
Resp Resp::simpleString(std::string s) {
    Resp r;
    r.value = std::move(s);
    r.type = RespType::SimpleString;
    return r;
}
Resp Resp::error(std::string s) {
    Resp r;
    r.value = std::move(s);
    r.type = RespType::Error;
    return r;
}
Resp Resp::integer(const int i) {
    Resp r;
    r.value = i;
    r.type = RespType::Integer;
    return r;
}
Resp Resp::bulkString(std::string s) {
    Resp r;
    r.value = std::move(s);
    r.type = RespType::BulkString;
    return r;
}
Resp Resp::array(RespVec arr) {
    Resp r;
    r.value = std::move(arr);
    r.type = RespType::Array;
    return r;
}
Resp Resp::nullBulkString() {
    Resp r;
    r.type = RespType::NullBS;
    return r;
}
Resp Resp::nullArray() {
    Resp r;
    r.type = RespType::NullArray;
    return r;
}


std::string Resp::encode() const {
    switch(type) {
        case RespType::SimpleString:
            return "+" + this->asString() + "\r\n";
        case RespType::Error:
            return "-" + this->asString() + "\r\n";
        case RespType::BulkString: {
            const std::string& str = this->asString();
            return "$" + std::to_string(str.length()) + "\r\n" + str + "\r\n";
        }
        case RespType::Integer:
            return ":" + std::to_string(this->asInt()) + "\r\n";
        case RespType::NullBS:
            return "$-1\r\n";
        case RespType::Array: {
            auto& arr = this->asArray();
            std::string res = "*" + std::to_string(arr.size()) + "\r\n";
            for (auto& el : arr) {
                res += el.encode();
            }
            return res;
        }
        default:
            return "*-1\r\n";
    }
}

// Return RESP error, simple string, and bulk string types as a string
const std::string& Resp::asString() const {
    if (type == RespType::Integer || type == RespType::Array) {
        throw std::runtime_error("Invalid RESP type, expected string");
    }
    return std::get<std::string>(value);
}

const RespVec& Resp::asArray() const {
    if (type != RespType::Array)
        throw std::runtime_error("Invalid RESP type, expected array");
    return std::get<RespVec>(value);
}

int Resp::asInt() const {
    if (type != RespType::Integer)
        throw std::runtime_error("Invalid RESP type, expected int");
    return std::get<int64_t>(value);
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
    if (pos >= data.size()) return std::nullopt; 
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
    while (pos < data.size() && data[pos] != '\r') {
        err.push_back(data[pos++]);
    }
    if (!expectCRLF()) return std::nullopt;
    return Resp::error(std::move(err));
}

std::optional<Resp> RespParser::parseBulkString() {
    // Invalid if empty or the first byte isn't '-' or '1'
    if (++pos >= data.size()) return std::nullopt;
    
    auto len = readInt(false);
    if (!len) return std::nullopt;
    if (*len == -1) return Resp::nullBulkString();

    // Process the actual string
    std::string str{};
    for (size_t i{0}; i < *len; ++i) {
        if (pos >= data.size()) break;
        str.push_back(data[pos++]);
    }

    if (str.length() != *len || !expectCRLF()) return std::nullopt;

    return Resp::bulkString(std::move(str));
}

std::optional<Resp> RespParser::parseSimpleString() {
    if (++pos >= data.size()) return std::nullopt;
    std::string str{};
    while (pos < data.size() && data[pos] != '\r') {
        str.push_back(data[pos++]);
    }

    if (!expectCRLF()) return std::nullopt;
    return Resp::simpleString(std::move(str));
}

std::optional<Resp> RespParser::parseArray() {
    if (++pos >= data.size()) return std::nullopt;
    auto len = readInt(false);
    if (!len || *len < -1) return std::nullopt;
    
    RespVec arr;
    arr.reserve(*len);
    for (size_t i {0}; i < *len; ++i) {
        if (pos >= data.size()) return std::nullopt;
        std::optional<Resp> r {parse()};
        if (!r) return std::nullopt;
        arr.push_back(*r);
    }

    if (*len != arr.size()) return std::nullopt;

    return Resp::array(std::move(arr));

}

std::optional<Resp> RespParser::parse() {
    if (pos >= data.size()) return std::nullopt;
    switch (data[pos]) {
        case '*': return parseArray();
        case '$': return parseBulkString();
        case '+': return parseSimpleString();
        case '-': return parseError();
        case ':': return parseInt();
        default:  return std::nullopt;
    }
}