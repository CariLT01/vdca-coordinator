cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(MyCMakeProject VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

add_compile_definitions(
    ASIO_STANDALONE
    _WEBSOCKETPP_CPP11_THREAD_
    _WEBSOCKETPP_CPP11_CHRONO_
    _WEBSOCKETPP_CPP11_FUNCTIONAL_
    _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
    _WEBSOCKETPP_CPP11_RANDOM_
    _WEBSOCKETPP_CPP11_MEMORY_
)

include(FetchContent)

FetchContent_Declare(
    asio_backend
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
)
FetchContent_MakeAvailable(asio_backend)

find_package(Threads REQUIRED)

if(NOT TARGET Boost::headers)
    add_library(Boost::headers INTERFACE IMPORTED GLOBAL)
    target_include_directories(Boost::headers INTERFACE ${asio_backend_SOURCE_DIR}/asio/include)
    target_compile_definitions(Boost::headers INTERFACE ASIO_STANDALONE BOOST_ASIO_STANDALONE)
    target_link_libraries(Boost::headers INTERFACE Threads::Threads)
endif()

if(NOT TARGET Boost::asio)
    add_library(Boost::asio INTERFACE IMPORTED GLOBAL)
    target_link_libraries(Boost::asio INTERFACE Boost::headers)
endif()

if(NOT TARGET Boost::system)
    add_library(Boost::system INTERFACE IMPORTED GLOBAL)
endif()

FetchContent_Declare(
    websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG        0.8.2 # Use the latest stable tag
)

FetchContent_GetProperties(websocketpp)
if(NOT websocketpp_POPULATED)
    FetchContent_Populate(websocketpp)
endif()

add_library(websocketpp_interface INTERFACE)
target_include_directories(websocketpp_interface INTERFACE ${websocketpp_SOURCE_DIR})

target_compile_definitions(websocketpp_interface INTERFACE 
    ASIO_STANDALONE 
    BOOST_ASIO_STANDALONE
    _WEBSOCKETPP_CPP11_THREAD_
    _WEBSOCKETPP_CPP11_CHRONO_
    _WEBSOCKETPP_CPP11_TYPE_TRAITS_
    _WEBSOCKETPP_CPP11_MEMORY_
    _WEBSOCKETPP_CPP11_FUNCTIONAL_
    _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
    _WEBSOCKETPP_CPP11_RANDOM_
)

set(SOURCES
    src/main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES}
    src/server.cpp
    src/server.h
    src/lockTracker.h
    src/lockTracker.cpp
    src/messageTypes.h
    src/packetUtils.h
    src/packetUtils.cpp)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
        websocketpp_interface
)

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::headers)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 mswsock)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)