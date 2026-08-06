add_executable(retrieval
  main.cpp
)

target_link_libraries(retrieval PRIVATE common_lib proto_lib)
