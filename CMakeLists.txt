cmake_minimum_required(VERSION 3.5...3.10)
project(network VERSION 1.0.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(package_name network)

add_subdirectory(extern)

# Create targets and set properties
add_library(${PROJECT_NAME}
    "src/Node.cpp"
)

add_library(${package_name}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} 
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"    
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"    
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_subdirectory(tests)
