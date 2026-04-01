#ifndef TINYIM_PROTOCOL_FRAMING_CODEC_H
#define TINYIM_PROTOCOL_FRAMING_CODEC_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace protocol {

class FramingCodec {
public:
    static constexpr std::uint32_t kMaxFrameSize = 1024 * 1024;

    static bool tryPopFrame(std::string& inbuf, std::string& frame, std::string& error);
    static std::string encode(const std::string& json_body);
};

}  // namespace protocol

#endif
