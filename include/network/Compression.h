#pragma once

#include <cstdint>
#include <memory>

#include <flatbuffers/flatbuffers.h>

namespace ntwk {

struct Image;

namespace compression {
    namespace none {
        std::shared_ptr<flatbuffers::DetachedBuffer> compressMsg(std::shared_ptr<flatbuffers::DetachedBuffer> msg);
        std::unique_ptr<uint8_t[]> decompressMsg(std::unique_ptr<uint8_t[]> msgBuffer);
    } // namespace none

    namespace image {
        namespace none {
            std::shared_ptr<flatbuffers::DetachedBuffer> compressMsg(unsigned int width, unsigned int height,
                                                                     uint8_t channels, const uint8_t data[]);
            std::unique_ptr<Image> decompressMsg(std::unique_ptr<uint8_t[]> msgBuffer);
        }

        namespace jpeg {
            std::shared_ptr<flatbuffers::DetachedBuffer> compressMsg(unsigned int width, unsigned int height,
                                                                     uint8_t channels, const uint8_t data[]);
            std::unique_ptr<Image> decompressMsg(std::unique_ptr<uint8_t[]> msgBuffer);
        } // namespace jpeg
    } // namespace img
} // namespace compression
} // namespace ntwk
