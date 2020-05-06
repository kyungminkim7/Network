#include <iostream>

#include <asio.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

#include <geometry_msgs/Vector3_generated.h>
#include <sensor_msgs/Image_generated.h>
#include <std_msgs/Header_generated.h>
#include <network/TcpSubscriber.h>

const auto window = "camera/image";

void onVector3Received(std::unique_ptr<uint8_t[]> vec3Buffer) {
    auto vec3 = geometry_msgs::GetVector3(vec3Buffer.get());
    std::cout << "(" << vec3->x() << ", " << vec3->y() << ", " << vec3->z() << ")\n";
}

std::unique_ptr<uint8_t[]> imgBufferGlobal;

void onImgReceived(std::unique_ptr<uint8_t[]> imgBuffer) {
    std::cout << "Received img\n";

    auto imgMsg = sensor_msgs::GetMutableImage(imgBuffer.get());
    std::cout << "Got img\n";

    std::cout << imgMsg->width() * imgMsg->height() << ", " << imgMsg->data()->size() << "\n";
    cv::Mat img(imgMsg->height(), imgMsg->width(), CV_8UC3, reinterpret_cast<void*>(imgMsg->mutable_data()->data()));

    imgBufferGlobal = std::move(imgBuffer);

    cv::imshow(window, img);
    cv::waitKey(1);
}

int main(int argc, char *argv[]) {
    asio::io_context ioContext;

    auto vec3Subscriber = ntwk::TcpSubscriber::create(ioContext, "127.0.0.1", 4444, &onVector3Received);
    auto headerSubscriber = ntwk::TcpSubscriber::create(ioContext, "127.0.0.1", 5555, [](auto headerBuffer){
        auto header = reinterpret_cast<std_msgs::Header*>(headerBuffer.get());
        std::cout << "Header: " << header->msgSize() << "\n";
    });

    auto imgSubscriber = ntwk::TcpSubscriber::create(ioContext, "127.0.0.1", 6666, &onImgReceived);

    cv::namedWindow(window);

    ioContext.run();

    cv::destroyAllWindows();

    return 0;
}
