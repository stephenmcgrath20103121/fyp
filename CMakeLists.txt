cmake_minimum_required(VERSION 3.15)
project(fyp_server VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Poco REQUIRED COMPONENTS Net Util Foundation JSON)
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(fyp_server Poco::Net Poco::Util Poco::Foundation Poco::JSON)