cmake_minimum_required(VERSION 3.10)
project(simple_server)

enable_testing()

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 在文件开头添加 OpenSSL 查找
find_package(OpenSSL REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED COMPONENTS serialization)

set(RAFT_KV_ROOT "${PROJECT_SOURCE_DIR}/../raft_KV")
find_path(RAFT_KV_CLERK_INCLUDE_DIR clerk.h
    PATHS "${RAFT_KV_ROOT}/src/raftClerk/include"
    NO_DEFAULT_PATH
)
set(RAFT_KV_INCLUDE_DIRS
    "${RAFT_KV_ROOT}/src/common/include"
    "${RAFT_KV_ROOT}/src/fiber/include"
    "${RAFT_KV_ROOT}/src/rpc/include"
    "${RAFT_KV_ROOT}/src/raftCore/include"
    "${RAFT_KV_ROOT}/src/raftRpcPro/include"
    "${RAFT_KV_ROOT}/src/raftClerk/include"
    "${RAFT_KV_ROOT}/src/skipList/include"
)
find_library(RAFT_KV_CLIENT_LIBRARY raft_kv_client
    PATHS "${RAFT_KV_ROOT}/lib"
    NO_DEFAULT_PATH
)
find_library(RAFT_KV_CORE_LIBRARY skip_list_on_raft
    PATHS "${RAFT_KV_ROOT}/lib"
    NO_DEFAULT_PATH
)
find_library(RAFT_KV_RPC_LIBRARY rpc_lib
    PATHS "${RAFT_KV_ROOT}/lib"
    NO_DEFAULT_PATH
)

if(NOT RAFT_KV_CLERK_INCLUDE_DIR OR NOT RAFT_KV_CLIENT_LIBRARY OR NOT RAFT_KV_CORE_LIBRARY OR NOT RAFT_KV_RPC_LIBRARY)
    message(FATAL_ERROR "Build raft_KV target raft_kv_client before configuring HTTPServer")
endif()

# 添加头文件路径
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/HttpServer/include
    ${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/include
    ${RAFT_KV_CLERK_INCLUDE_DIR}
    /usr/include/mysql-cppconn-8
    /usr/include/mysql
    ${OPENSSL_INCLUDE_DIR}
)

# 查找必要的库
find_library(MYSQLCPPCONN_LIBRARY
    NAMES mysqlcppconn mysqlcppconn8
    PATHS /usr/lib /usr/lib64 /usr/local/lib
)

# 添加所有源文件
file(GLOB_RECURSE HTTP_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/HttpServer/src/*.cpp"
)

file(GLOB_RECURSE GOMOKU_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/src/*.cpp"
)

# 添加主程序文件
set(MAIN_SRC "${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/src/main.cpp")

# 如果主程序文件不存在，创建一个
if(NOT EXISTS ${MAIN_SRC})
    file(WRITE ${MAIN_SRC}
"#include \"../include/GomokuServer.h\"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        GomokuServer server;
        server.start();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << \"Error: \" << e.what() << std::endl;
        return 1;
    }
}
")
endif()

# 添加可执行文件
add_executable(simple_server
    ${MAIN_SRC}
    ${HTTP_SERVER_SRC}
    ${GOMOKU_SERVER_SRC}
)

# 链接必要的库
target_link_libraries(simple_server
    pthread
    muduo_net
    muduo_base
    mysqlcppconn
    mysqlclient
    ssl
    crypto
    ${RAFT_KV_CLIENT_LIBRARY}
    ${RAFT_KV_CORE_LIBRARY}
    ${RAFT_KV_RPC_LIBRARY}
    protobuf::libprotobuf
    Boost::serialization
)

target_include_directories(simple_server PRIVATE ${RAFT_KV_INCLUDE_DIRS})

add_executable(game_room_test
    WebApps/GomokuServer/test/GameRoomTest.cpp
    WebApps/GomokuServer/src/GameRoom.cpp
)

target_include_directories(game_room_test PRIVATE WebApps/GomokuServer/include)

add_test(NAME game_room_test COMMAND game_room_test)

add_executable(pvp_game_service_test
    WebApps/GomokuServer/test/test_pvp_game_service.cpp
    WebApps/GomokuServer/src/GameRoom.cpp
    WebApps/GomokuServer/src/PvpGameService.cpp
)

target_include_directories(pvp_game_service_test PRIVATE WebApps/GomokuServer/include)

add_test(NAME pvp_game_service_test COMMAND pvp_game_service_test)

add_executable(raft_game_state_store_test
    WebApps/GomokuServer/test/test_raft_game_state_store.cpp
    WebApps/GomokuServer/src/GameRoom.cpp
    WebApps/GomokuServer/src/PvpGameService.cpp
    WebApps/GomokuServer/src/RaftGameStateStore.cpp
)

target_include_directories(raft_game_state_store_test PRIVATE WebApps/GomokuServer/include)

add_test(NAME raft_game_state_store_test COMMAND raft_game_state_store_test)

add_executable(game_ws_handler_test
    WebApps/GomokuServer/test/test_game_ws_handler.cpp
    WebApps/GomokuServer/src/gameWsMessages.cpp
    WebApps/GomokuServer/src/gameWsMoveFlow.cpp
    WebApps/GomokuServer/src/GameRoom.cpp
    WebApps/GomokuServer/src/PvpGameService.cpp
)

target_include_directories(game_ws_handler_test PRIVATE WebApps/GomokuServer/include)

add_test(NAME game_ws_handler_test COMMAND game_ws_handler_test)

find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    add_test(NAME pvp_frontend_smoke_test
        COMMAND ${NODE_EXECUTABLE} ${PROJECT_SOURCE_DIR}/WebApps/GomokuServer/test/pvp_frontend_smoke_test.js)
endif()

# 打印调试信息
message(STATUS "Include directories:")
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
    message(STATUS "  ${dir}")
endforeach()

message(STATUS "Main source file: ${MAIN_SRC}")
message(STATUS "HTTP Server sources: ${HTTP_SERVER_SRC}")
message(STATUS "Gomoku Server sources: ${GOMOKU_SERVER_SRC}")
