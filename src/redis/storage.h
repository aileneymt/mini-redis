#ifndef STORAGE_H
#define STORAGE_H

#include <stdexcept>
#include <chrono>
#include <optional>
#include <variant>
#include <deque> 
#include <string>

using StringList = std::deque<std::string>;

struct StorageEntry {
    std::variant<std::string, StringList> value;
    std::optional<std::chrono::time_point<std::chrono::steady_clock>> expiry;

    bool isString() {
        return std::holds_alternative<std::string>(value);
    }
    bool isList() {
        return std::holds_alternative<StringList>(value);
    }

    bool isExpired() {
        return expiry.has_value() && std::chrono::steady_clock::now() > *expiry;
    }

    std::string& asString() {
        if (!isString()) throw std::runtime_error("value type is not std::string");
        return std::get<std::string>(value);
    }

    StringList& asArray() {
        if (!isList()) throw std::runtime_error("value type is not std::vector");
        return std::get<StringList>(value);
    }
};

#endif