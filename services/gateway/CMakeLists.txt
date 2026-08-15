add_executable(gateway
  main.cpp
)

target_include_directories(gateway PRIVATE /opt/homebrew/include)
target_link_libraries(gateway PRIVATE common_lib proto_lib Threads::Threads)
