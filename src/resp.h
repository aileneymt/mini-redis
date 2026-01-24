#ifndef RESP_H
#define RESP_H

#include <cstdint>
#include <vector>
#include <variant>
#include <string>
#include <optional>

class Resp; // forward declare
using u8 = uint8_t;
using RespVec = std::vector<Resp>;

enum class RespType {
    SimpleString,
    Error,
    Integer,
    BulkString, // $<length>\r\n<data>\r\n
    Array,
    Null
};

class Resp {
private:
    std::variant<
        std::string,
        int64_t,
        RespVec
    > value;


        
public:
    RespType type;
    // Constructs a Resp of a specific type given the data
    // TODO: determine if its better to PBR or PBV here
    static Resp simpleString(std::string s);
    static Resp error(std::string s);
    static Resp integer(const int i);
    static Resp bulkString(std::string s);
    static Resp array(RespVec arr);
    static Resp nullBulkString();
    
    // Encode to RESP format
    std::string encode() const;
    
    const std::string& asString() const;
    const RespVec& asArray() const;
    int asInt() const;
};

class RespParser {
    const std::vector<u8>& data{};
    size_t pos = 0;

    std::optional<Resp> parseArray();
    std::optional<Resp> parseInt();
    std::optional<Resp> parseError();
    std::optional<Resp> parseBulkString();
    std::optional<Resp> parseSimpleString();

    bool expectCRLF();
    std::optional<int> readInt(bool posOk=true);

public:
    RespParser(const std::vector<u8>& bytes) : data{bytes}
    {
    }

    std::optional<Resp> parse();
    bool bufferEmpty() { return pos == data.size(); }
};

#endif