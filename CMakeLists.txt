cmake_minimum_required(VERSION 3.10)

project(TinyIM VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(tinyim_v4
    src/main.cpp
    src/server.cpp
    protocol/FramingCodec.cpp
    protocol/JsonCodec.cpp
    storage/SqliteDB.cpp
    storage/UserDao.cpp
    service/AuthService.cpp
    service/SessionManager.cpp
)

target_link_libraries(tinyim_v4 PRIVATE sqlite3)
