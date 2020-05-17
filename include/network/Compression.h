#pragma once

#include <cstdint>
#include <memory>

#include <flatbuffers/flatbuffers.h>

namespace ntwk {

enum class Compression {NONE, ZLIB, JPEG};

namespace zlib {

std::shared_ptr<flatbuffers::DetachedBuffer> compressMsg(flatbuffers::DetachedBuffer *msg);
std::unique_ptr<uint8_t[]> decompressMsg(uint8_t compressedMsgBuffer[]);

} // namespace zlib

namespace jpeg {

std::shared_ptr<flatbuffers::DetachedBuffer> compressImage(unsigned int width, unsigned int height,
                                                           uint8_t channels, const uint8_t data[]);

} // namespace jpeg

} // namespace ntwk
