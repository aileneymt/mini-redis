#ifndef STORAGE_H
#define STORAGE_H

#include <stdexcept>
#include <chrono>
#include <optional>
#include <variant>
#include <deque> 
#include <string>

using StringList = std::deque<std::string>;

enum class StorageType {
    String,
    List,
    // Future: Set, ZSet, Hash, Stream, etc.
};

struct StorageEntry {
    std::variant<std::string, StringList> value;
    StorageType type = StorageType::String;
    std::optional<std::chrono::time_point<std::chrono::steady_clock>> expiry;
    

    /*
    bool isString() {
        return std::holds_alternative<std::string>(value);
    }
    bool isList() {
        return std::holds_alternative<StringList>(value);
    }
    */

    std::string getTypeName() {
        switch(type) {
            case StorageType::String:
                return "string";
            case StorageType::List:
                return "list";
            default:
                return "NOT IMPLEMENTED";
        }
    }

    bool isExpired() {
        return expiry.has_value() && std::chrono::steady_clock::now() > *expiry;
    }

    std::string& asString() {
        if (type != StorageType::String) throw std::runtime_error("value type is not std::string");
        return std::get<std::string>(value);
    }

    StringList& asList() {
        if (type != StorageType::List) throw std::runtime_error("value type is not StringList");
        return std::get<StringList>(value);
    }

};

#endif