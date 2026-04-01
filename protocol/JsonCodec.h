#ifndef TINYIM_PROTOCOL_JSON_CODEC_H
#define TINYIM_PROTOCOL_JSON_CODEC_H

#include <string>
#include <map>

namespace protocol {

class JsonCodec {
public:
    static bool isValidMessage(const std::string& json_text, std::string& error);
    static bool parseObject(
        const std::string& json_text,
        std::map<std::string, std::string>& out,
        std::string& error
    );
    static std::string encodeObject(const std::map<std::string, std::string>& fields);

private:
    static bool parseString(const std::string& text, std::size_t& pos, std::string& out);
    static void skipWhitespace(const std::string& text, std::size_t& pos);
    static std::string escapeString(const std::string& value);
};

}  // namespace protocol

#endif
