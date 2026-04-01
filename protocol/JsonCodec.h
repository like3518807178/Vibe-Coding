#ifndef TINYIM_PROTOCOL_JSON_CODEC_H
#define TINYIM_PROTOCOL_JSON_CODEC_H

#include <string>

namespace protocol {

class JsonCodec {
public:
    // V2 only accepts a minimal JSON subset:
    // - top-level object
    // - string keys
    // - string values
    static bool isValidMessage(const std::string& json_text, std::string& error);

private:
    static bool parseString(const std::string& text, std::size_t& pos, std::string& out);
    static void skipWhitespace(const std::string& text, std::size_t& pos);
};

}  // namespace protocol

#endif
