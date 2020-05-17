#include <sensor_msgs/ImageMsgBuilder.h>

namespace sensor_msgs {

std::shared_ptr<flatbuffers::DetachedBuffer> buildImageMsg(unsigned int width, unsigned int height,
                                                           uint8_t channels, const uint8_t data[]) {

    flatbuffers::FlatBufferBuilder imgMsgBuilder;
    auto imgMsgData = imgMsgBuilder.CreateVector(data, width * height * channels);
    auto imgMsg = sensor_msgs::CreateImage(imgMsgBuilder, width, height, channels, imgMsgData);
    imgMsgBuilder.Finish(imgMsg);
    return std::make_shared<flatbuffers::DetachedBuffer>(imgMsgBuilder.Release());
}

} // namespace sensor_msgs
