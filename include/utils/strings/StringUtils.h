#pragma once

#include <string>
#include <cctype>
#include <algorithm>

namespace tsm { namespace utils {

inline std::string ToLower(std::string str) {
    for (auto& ch : str) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return str;
}

inline std::string ToUpper(std::string str) {
    for (auto& ch : str) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return str;
}

inline std::string Capitalize(std::string str) {
    if (!str.empty()) {
        str[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(str[0])));
        for (size_t i = 1; i < str.size(); ++i) {
            str[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(str[i])));
        }
    }
    return str;
}

inline void ReplaceAll(std::string& str, char from, char to) {
    for (auto& ch : str) {
        if (ch == from) ch = to;
    }
}

inline std::string RemoveDigits(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            result.push_back(c);
        }
    }
    return result;
}

inline std::string Trim(const std::string& str) {
    size_t start = 0;
    while (start < str.size() && str[start] == ' ') ++start;
    size_t end = str.size();
    while (end > start && str[end - 1] == ' ') --end;
    return str.substr(start, end - start);
}

inline std::string RemoveDuplicateSpaces(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    bool prevSpace = false;
    for (char c : str) {
        if (c == ' ') {
            if (!prevSpace) result.push_back(c);
            prevSpace = true;
        } else {
            result.push_back(c);
            prevSpace = false;
        }
    }
    return result;
}

inline std::string InsertSpacesBeforeCaps(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 8);
    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];
        if (i > 0) {
            char prev = str[i - 1];
            char next = (i + 1 < str.size()) ? str[i + 1] : '\0';
            bool needSpace = (std::isupper(static_cast<unsigned char>(c)) &&
                            std::islower(static_cast<unsigned char>(prev)) &&
                            prev != ' ') ||
                            (std::isupper(static_cast<unsigned char>(c)) &&
                            std::isupper(static_cast<unsigned char>(prev)) &&
                            (i + 1 < str.size()) &&
                            std::islower(static_cast<unsigned char>(next)));
            if (needSpace) result.push_back(' ');
        }
        result.push_back(c);
    }
    return result;
}

inline std::string CapitalizeWords(std::string str) {
    bool capitalize = true;
    for (char& c : str) {
        if (capitalize && std::isalpha(static_cast<unsigned char>(c))) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            capitalize = false;
        } else if (c == ' ') {
            capitalize = true;
        }
    }
    return str;
}

inline std::string RemovePrefix(const std::string& str, const std::string& prefix) {
    if (str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0) {
        return str.substr(prefix.size());
    }
    return str;
}

inline std::string RemoveSuffix(const std::string& str, const std::string& suffix) {
    if (str.size() >= suffix.size() &&
        str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return str.substr(0, str.size() - suffix.size());
    }
    return str;
}

inline bool StartsWithIgnoreCase(const std::string& str, const std::string& prefix) {
    if (str.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(str[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

}}
