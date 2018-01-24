cmake_minimum_required(VERSION 3.7)
project(ADCManager)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES src/ADCManager.cpp)
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin CACHE PATH "Executable output path")

# POCO
set(ENABLE_MONGODB false)
set(ENABLE_DATA false)
set(ENABLE_ZIP false)
set(ENABLE_PAGECOMPILER false)
set(ENABLE_PAGECOMPILER_FILE2PAGE false)
set(ENABLE_XML false)
set(ENABLE_NET false)
set(ENABLE_UTIL true)

set(CMAKE_MODULE_PATH
        ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(Poco REQUIRED JSON Util Foundation)

add_executable(adcmanager ${SOURCE_FILES})

find_package(EventQueue REQUIRED)

target_link_libraries(adcmanager PUBLIC ${EventQueue_LIBRARY})
target_link_libraries(adcmanager PUBLIC ${Poco_Foundation_LIBRARY})
target_link_libraries(adcmanager PUBLIC ${Poco_Util_LIBRARY})
target_link_libraries(adcmanager PUBLIC ${Poco_JSON_LIBRARY})
target_link_libraries(adcmanager PUBLIC ${EventQueue_LIBRARY})

