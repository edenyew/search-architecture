add_executable(controller
  main.cpp
  circuit_breaker.cpp
)

target_link_libraries(controller PRIVATE common_lib proto_lib)
