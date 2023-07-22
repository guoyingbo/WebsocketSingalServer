cmake_minimum_required(VERSION 2.8)

project(wsSignalServer)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")


aux_source_directory(. DIR_SRCS)
include_directories(./jsoncpp/include ./openssl/include)
add_subdirectory(jsoncpp)

add_definitions("-Wall -g")

add_executable(wsSignalServer ${DIR_SRCS} message_queue.cpp message_queue.h)

find_package(Boost 1.72 REQUIRED COMPONENTS log_setup log)
find_library(LIB_SSL libssl.a REQUIRED PATHS ./openssl/lib)
find_library(LIB_CRYPTO libcrypto.a REQUIRED PATHS ./openssl/lib)

message(${LIB_SSL})

target_link_libraries(wsSignalServer jsoncpp pthread  ${Boost_LIBRARIES} ${LIB_SSL} ${LIB_CRYPTO}	)

set(CMAKE_INSTALL_PREFIX /usr)
install(FILES "${CMAKE_SOURCE_DIR}/wsSignalServer.service"
		DESTINATION /lib/systemd/system)

#systemctl daemon-reload
#systemctl eanble wsSignalServer.service
#systemctl start wsSignalServer.service
#systemctl status wsSignalServer.service
#systemctl stop wsSignalServer.service

install(TARGETS wsSignalServer
		RUNTIME DESTINATION bin)
