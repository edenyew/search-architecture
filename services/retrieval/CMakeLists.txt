add_executable(retrieval
  main.cpp
  ann_index.cpp
  merge.cpp
  thread_pool.cpp
)

target_include_directories(retrieval PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(retrieval PRIVATE common_lib proto_lib Threads::Threads)
