cmake_minimum_required(VERSION 3.29)

set(CMAKE_CXX_STANDARD 20)

project(blockchain)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")

add_library(blockchain-lib lib/broadcast.cpp lib/simulation.cpp lib/sha256.cpp)
target_include_directories(blockchain-lib PUBLIC lib)
target_link_options(blockchain-lib PRIVATE -Wl,--gc-sections)

add_executable(blockchain main.cpp)
target_link_libraries(blockchain PUBLIC blockchain-lib)
target_link_options(blockchain PRIVATE -Wl,--gc-sections)

add_executable(simulation test.cpp)
target_link_libraries(simulation PUBLIC blockchain-lib)
target_link_options(simulation PRIVATE -Wl,--gc-sections)

enable_testing()

install(TARGETS blockchain DESTINATION bin)
