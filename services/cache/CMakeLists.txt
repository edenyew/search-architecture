add_executable(cache
  main.cpp
)

target_include_directories(cache PRIVATE /opt/homebrew/include)
target_link_libraries(cache PRIVATE common_lib proto_lib ${REDISPLUSPLUS_LIB} ${HIREDIS_LIB})
