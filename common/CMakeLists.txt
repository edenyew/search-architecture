add_library(common_lib STATIC
  corpus.cpp
  index.cpp
  tokenizer.cpp
)

target_include_directories(common_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(common_lib PUBLIC nlohmann_json::nlohmann_json)
