#pragma once

#include <cstdint>
#include <memory>

#include <flatbuffers/flatbuffers.h>

namespace ntwk {

enum class Compression {NONE, ZLIB, JPEG};

namespace zlib {

std::shared_ptr<flatbuffers::DetachedBuffer> compressMsg(std::shared_ptr<flatbuffers::DetachedBuffer> msg);
std::unique_ptr<uint8_t[]> decompressMsg(std::unique_ptr<uint8_t[]> compressedMsgBuffer);

} // namespace zlib

namespace jpeg {

} // namespace jpeg

} // namespace ntwk
