include_directories(./include)
aux_source_directory(./src JSON_DIRSRCS)
add_library(jsoncpp ${JSON_DIRSRCS})
