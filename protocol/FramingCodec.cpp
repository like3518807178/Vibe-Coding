#include "FramingCodec.h"

#include <arpa/inet.h>
#include <cstring>

namespace protocol {

namespace {

std::uint32_t& globalMaxFrameSize() {
    static std::uint32_t value = 1024 * 1024;
    return value;
}

}  // namespace

void FramingCodec::setMaxFrameSize(std::uint32_t max_frame_size) {
    globalMaxFrameSize() = max_frame_size;
}

std::uint32_t FramingCodec::maxFrameSize() {
    return globalMaxFrameSize();
}

bool FramingCodec::tryPopFrame(std::string& inbuf, std::string& frame, std::string& error) {
    frame.clear();
    error.clear();

    if (inbuf.size() < sizeof(std::uint32_t)) {
        return false;
    }

    std::uint32_t body_len_network = 0;
    std::memcpy(&body_len_network, inbuf.data(), sizeof(body_len_network));

    const std::uint32_t body_len = ntohl(body_len_network);
    if (body_len > maxFrameSize()) {
        error = "frame length exceeds limit";
        return false;
    }

    const std::size_t total_len = sizeof(std::uint32_t) + body_len;
    if (inbuf.size() < total_len) {
        return false;
    }

    frame = inbuf.substr(sizeof(std::uint32_t), body_len);
    inbuf.erase(0, total_len);
    return true;
}

std::string FramingCodec::encode(const std::string& json_body) {
    const std::uint32_t body_len = static_cast<std::uint32_t>(json_body.size());
    const std::uint32_t body_len_network = htonl(body_len);

    std::string packet(sizeof(std::uint32_t), '\0');
    std::memcpy(&packet[0], &body_len_network, sizeof(body_len_network));
    packet += json_body;
    return packet;
}

}  // namespace protocol
