#include <iostream>
#include <memory>

#include <asio.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include <geometry_msgs/Vector3_generated.h>
#include <sensor_msgs/Image_generated.h>
#include <std_msgs/Header_generated.h>
#include <network/TcpPublisher.h>

int main(int argc, char *argv[]) {
    asio::io_context ioContext;

    auto vec3Publisher = ntwk::TcpPublisher::create(ioContext, 4444);
    auto headerPublisher = ntwk::TcpPublisher::create(ioContext, 5555);
    auto imgPublisher = ntwk::TcpPublisher::create(ioContext, 6666);

    cv::VideoCapture cam(2);
    if (!cam.isOpened()) {
        std::cout << "Failed to open camera\n";
        return 1;
    }

    std::cout << "Opened camera\n";

    int x;
    do {
        flatbuffers::FlatBufferBuilder msgBuilder(64);
        auto vec = geometry_msgs::CreateVector3(msgBuilder, 1.1f, 2.2f, 3.3f);
        msgBuilder.Finish(vec);

        vec3Publisher->publish(std::make_shared<flatbuffers::DetachedBuffer>(msgBuilder.Release()));
        headerPublisher->publish(std::make_shared<std_msgs::Header>(5849));

        // Image
        cv::Mat img;
        if (cam.read(img)) {
            flatbuffers::FlatBufferBuilder imgMsgBuilder;
            auto imgData = imgMsgBuilder.CreateVector(img.data, img.total() * img.channels());
            auto imgMsg = sensor_msgs::CreateImage(imgMsgBuilder, img.cols, img.rows,
                                                   sensor_msgs::Encoding::BGR8, imgData);
            imgMsgBuilder.Finish(imgMsg);

            imgPublisher->publish(std::make_shared<flatbuffers::DetachedBuffer>(imgMsgBuilder.Release()));
        }

        ioContext.poll();
        ioContext.restart();
    } while (true);

    return 0;
}
