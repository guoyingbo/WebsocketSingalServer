cmake_minimum_required(VERSION 2.8)

project(wsSignalServer)


aux_source_directory(. DIR_SRCS)
include_directories(./jsoncpp/include)
add_subdirectory(jsoncpp)

add_executable(wsSignalServer ${DIR_SRCS} message_queue.cpp message_queue.h)

find_package(Boost 1.72 REQUIRED COMPONENTS log_setup log)

target_link_libraries(wsSignalServer jsoncpp pthread rt ${Boost_LIBRARIES}	)

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
