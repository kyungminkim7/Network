#pragma once

#include <cstdint>
#include <memory>

#include "Image_generated.h"

namespace sensor_msgs {

std::shared_ptr<flatbuffers::DetachedBuffer> buildImageMsg(unsigned int width, unsigned int height,
                                                           uint8_t channels, const uint8_t data[]);

} // namespace sensor_msgs
