cmake_minimum_required(VERSION 3.20)

project(PhotoFrameTests LANGUAGES CXX)

enable_testing()

add_executable(
    card_initialization_tests
    CardInitializationTests.cpp
    ../CardDriver/Card.cpp
    ../CardDriver/SystemCard.cpp
    ../CardDriver/CardProtocol.cpp
    ../CardDriver/SD_V1.cpp
    ../CardDriver/SD_V2.cpp
)

target_compile_features(card_initialization_tests PRIVATE cxx_std_20)
target_compile_options(card_initialization_tests PRIVATE -Wall -Wextra -Werror)
target_include_directories(
    card_initialization_tests
    PRIVATE
    ../CardDriver
    ../CardDriver/FatFs/Inc
    ../Indicate
)

add_test(NAME card_initialization COMMAND card_initialization_tests)
