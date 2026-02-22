cmake_minimum_required(VERSION 3.16)
project(PlutoSDR CXX)

set(CMAKE_BUILD_TYPE Debug)

option(INSTALL_DEPS "Установить зависимости" ON)
option(PLUTO_TIMESTAMP "Build pluto with timestamp" ON)

find_package(SoapySDR REQUIRED)

set(MAIN_SOURCE_FILES
    main.cpp
    SDR_init.cpp 
)

set(TEST_SOURCE_FILES
    test.cpp
)

add_executable(main ${MAIN_SOURCE_FILES})

target_include_directories(main PRIVATE 
    ${SoapySDR_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(main ${SoapySDR_LIBRARIES} m)

set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_executable(test ${TEST_SOURCE_FILES})
target_include_directories(test PRIVATE 
    ${SoapySDR_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(test ${SoapySDR_LIBRARIES} m)
set_target_properties(test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# для полной очистки make distclean
add_custom_target(distclean
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeTmp
    COMMENT "Очистка кэша CMake и временных файлов..."
)