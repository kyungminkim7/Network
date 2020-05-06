find_package(OpenCV REQUIRED COMPONENTS
    core
    highgui
    imgcodecs
    videoio
)

add_executable(Server Server.cpp)
target_link_libraries(Server PRIVATE
    opencv_core
    opencv_imgcodecs
    opencv_videoio
    network::network
)

add_executable(Client Client.cpp)
target_link_libraries(Client PRIVATE
    opencv_core
    opencv_imgcodecs
    opencv_videoio
    opencv_highgui
    network::network
)
