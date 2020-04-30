add_executable(Server Server.cpp)
target_link_libraries(Server PRIVATE network::network)

add_executable(Client Client.cpp)
target_link_libraries(Client PRIVATE network::network)
