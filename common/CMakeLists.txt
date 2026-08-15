add_library(common_lib STATIC
  corpus.cpp
  index.cpp
  tokenizer.cpp
  wordpiece_tokenizer.cpp
  embedder.cpp
  embedding_store.cpp
  trace.cpp
)

target_include_directories(common_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} /opt/homebrew/include/onnxruntime)
target_link_libraries(common_lib PUBLIC nlohmann_json::nlohmann_json ${ONNXRUNTIME_LIB} gRPC::grpc++)
