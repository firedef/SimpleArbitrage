cmake_minimum_required(VERSION 3.24)
project(arb)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(RAPIDJSON ${CMAKE_SOURCE_DIR}/include/rapidjson)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
FIND_PACKAGE(Boost COMPONENTS system filesystem)
FIND_PACKAGE(OpenSSL)
if (NOT MSVC)
    link_libraries(${OPENSSL_LIBRARIES})
endif()

include_directories(${RAPIDJSON} ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
#include_directories( ${RAPIDJSON} ${Boost_INCLUDE_DIR} )


add_executable(arb main.cpp src/web.h src/arbitrage.h)
#target_link_libraries(arb OpenSSL::SSL OpenSSL::Crypto)
#target_link_libraries(arb lib-asio lib-beast)

