cmake_minimum_required(VERSION 3.16)
project(PlutoSDR C)

set(CMAKE_BUILD_TYPE Debug)

# Опции конфигурации
option(INSTALL_DEPS "Установить зависимости" ON)
option(PLUTO_TIMESTAMP "Build pluto with timestamp" ON)

# Поиск библиотеки SoapySDR
find_package(SoapySDR REQUIRED)

# Все исходные файлы проекта
set(MAIN_SOURCE_FILES
    main.c
    SDR_init.c
    
)

# Создаем исполняемый файл
add_executable(main ${MAIN_SOURCE_FILES})

# Подключаем заголовочные файлы
target_include_directories(main PRIVATE 
    ${SoapySDR_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Линкуем библиотеки
target_link_libraries(main ${SoapySDR_LIBRARIES})

# Установка выходного файла в папку build
set_target_properties(main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)