add_executable(reranker
  main.cpp
)

target_link_libraries(reranker PRIVATE common_lib proto_lib)
