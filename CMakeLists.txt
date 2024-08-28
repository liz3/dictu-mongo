cmake_minimum_required(VERSION 3.16.3)
PROJECT("dictu-mongo")
set(CMAKE_C_STANDARD 11)
add_library(dictu-mongo SHARED src/dictu-mongo.c)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(third-party/mongo-c-driver)

target_include_directories(dictu-mongo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third-party/dictu/")
target_include_directories(dictu-mongo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third-party/mongo-c-driver/src/libmongoc/src")
target_include_directories(dictu-mongo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third-party/mongo-c-driver/src/libbson/src")

target_link_libraries(dictu-mongo PRIVATE bson_static mongoc_static)