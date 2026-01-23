#include <cstdint>
#include <vector>
#include <variant>
#include <string>
#include <optional>

using u8 = uint8_t;

enum class RespType {
    SimpleString,
    Error,
    Integer,
    BulkString, // $<length>\r\n<data>\r\n
    Array,
};

class Resp {
private:
    std::variant<
        std::string,
        int,
        std::vector<Resp>
    > value;
        
public:
    RespType type;
    // Constructs a Resp of a specific type given the data
    // TODO: determine if its better to PBR or PBV here
    static Resp simpleString(const std::string& s);
    static Resp error(const std::string& s);
    static Resp integer(const int i);
    static Resp bulkString(const std::string& s);
    static Resp array(const std::vector<Resp>& arr);
    static Resp nullBulkString();
    
    // Encode to RESP format
    std::string encode() const;

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
};