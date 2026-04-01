#include "JsonCodec.h"

#include <cctype>
#include <sstream>

namespace protocol {

void JsonCodec::skipWhitespace(const std::string& text, std::size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
}

bool JsonCodec::parseString(const std::string& text, std::size_t& pos, std::string& out) {
    out.clear();
    if (pos >= text.size() || text[pos] != '"') {
        return false;
    }

    ++pos;
    while (pos < text.size()) {
        const char ch = text[pos++];
        if (ch == '"') {
            return true;
        }

        if (ch == '\\') {
            if (pos >= text.size()) {
                return false;
            }

            const char escaped = text[pos++];
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(escaped);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                default:
                    return false;
            }
            continue;
        }

        if (static_cast<unsigned char>(ch) < 0x20) {
            return false;
        }

        out.push_back(ch);
    }

    return false;
}

bool JsonCodec::isValidMessage(const std::string& json_text, std::string& error) {
    std::map<std::string, std::string> parsed;
    return parseObject(json_text, parsed, error);
}

bool JsonCodec::parseObject(
    const std::string& json_text,
    std::map<std::string, std::string>& out,
    std::string& error
) {
    out.clear();
    error.clear();

    std::size_t pos = 0;
    skipWhitespace(json_text, pos);
    if (pos >= json_text.size() || json_text[pos] != '{') {
        error = "JSON must start with object";
        return false;
    }

    ++pos;
    skipWhitespace(json_text, pos);
    if (pos >= json_text.size()) {
        error = "JSON object is incomplete";
        return false;
    }

    if (json_text[pos] == '}') {
        ++pos;
        skipWhitespace(json_text, pos);
        if (pos != json_text.size()) {
            error = "unexpected trailing data";
            return false;
        }
        return true;
    }

    while (pos < json_text.size()) {
        std::string key;
        if (!parseString(json_text, pos, key)) {
            error = "invalid JSON key";
            return false;
        }

        skipWhitespace(json_text, pos);
        if (pos >= json_text.size() || json_text[pos] != ':') {
            error = "missing colon after key";
            return false;
        }

        ++pos;
        skipWhitespace(json_text, pos);

        std::string value;
        if (!parseString(json_text, pos, value)) {
            error = "JSON is valid in general, but V3 currently only supports string values";
            return false;
        }

        out[key] = value;

        skipWhitespace(json_text, pos);
        if (pos >= json_text.size()) {
            error = "JSON object is incomplete";
            return false;
        }

        if (json_text[pos] == '}') {
            ++pos;
            skipWhitespace(json_text, pos);
            if (pos != json_text.size()) {
                error = "unexpected trailing data";
                return false;
            }
            return true;
        }

        if (json_text[pos] != ',') {
            error = "missing comma between fields";
            return false;
        }

        ++pos;
        skipWhitespace(json_text, pos);
    }

    error = "JSON object is incomplete";
    return false;
}

std::string JsonCodec::escapeString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped.push_back(ch);
                break;
        }
    }

    return escaped;
}

std::string JsonCodec::encodeObject(const std::map<std::string, std::string>& fields) {
    std::ostringstream oss;
    oss << "{";

    bool first = true;
    for (const auto& entry : fields) {
        if (!first) {
            oss << ",";
        }

        first = false;
        oss << "\"" << escapeString(entry.first) << "\":"
            << "\"" << escapeString(entry.second) << "\"";
    }

    oss << "}";
    return oss.str();
}

}  // namespace protocol
