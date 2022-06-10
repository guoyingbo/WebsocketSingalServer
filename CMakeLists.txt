cmake_minimum_required (VERSION 2.8)

project(WebsocketSignalServer)

# 并将名称保存到 DIR_SRCS 变量
aux_source_directory(. DIR_SRCS)

# 指定生成目标

add_executable(WebsocketSignalServer ${DIR_SRCS})

target_link_libraries(WebsocketSignalServer jsoncpp)
