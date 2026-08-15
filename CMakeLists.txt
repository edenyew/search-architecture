cmake_minimum_required(VERSION 3.20)
project(search_backend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# redis++'s CMake package requires hiredis to have one too, but
# Homebrew's hiredis only ships a plain library + pkg-config file
# (same gap as Crow/Asio) — so locate both directly instead.
find_library(REDISPLUSPLUS_LIB redis++ REQUIRED)
find_library(HIREDIS_LIB hiredis REQUIRED)

# onnxruntime also has no CMake package — same situation.
find_library(ONNXRUNTIME_LIB onnxruntime REQUIRED)

# --- Protobuf/gRPC codegen -------------------------------------------------
#
# common.proto declares messages only (no service), so it only needs
# --cpp_out. The rest declare a service and need --grpc_out too — the
# grpc plugin produces no output for a file with no service, so it must
# not be invoked on common.proto.

set(PROTO_SRC_DIR ${CMAKE_SOURCE_DIR}/proto)
set(PROTO_GEN_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(MESSAGE_ONLY_PROTOS common)
set(SERVICE_PROTOS retrieval controller cache reranker)

set(ALL_PROTO_GENERATED_SRCS)

foreach(proto_name ${MESSAGE_ONLY_PROTOS})
  set(pb_cc "${PROTO_GEN_DIR}/${proto_name}.pb.cc")
  set(pb_h "${PROTO_GEN_DIR}/${proto_name}.pb.h")
  add_custom_command(
    OUTPUT ${pb_cc} ${pb_h}
    COMMAND protobuf::protoc
    ARGS --cpp_out=${PROTO_GEN_DIR}
         -I ${PROTO_SRC_DIR}
         ${PROTO_SRC_DIR}/${proto_name}.proto
    DEPENDS ${PROTO_SRC_DIR}/${proto_name}.proto
    COMMENT "Generating protobuf code for ${proto_name}.proto"
  )
  list(APPEND ALL_PROTO_GENERATED_SRCS ${pb_cc})
endforeach()

foreach(proto_name ${SERVICE_PROTOS})
  set(pb_cc "${PROTO_GEN_DIR}/${proto_name}.pb.cc")
  set(pb_h "${PROTO_GEN_DIR}/${proto_name}.pb.h")
  set(grpc_pb_cc "${PROTO_GEN_DIR}/${proto_name}.grpc.pb.cc")
  set(grpc_pb_h "${PROTO_GEN_DIR}/${proto_name}.grpc.pb.h")
  add_custom_command(
    OUTPUT ${pb_cc} ${pb_h} ${grpc_pb_cc} ${grpc_pb_h}
    COMMAND protobuf::protoc
    ARGS --cpp_out=${PROTO_GEN_DIR}
         --grpc_out=${PROTO_GEN_DIR}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         -I ${PROTO_SRC_DIR}
         ${PROTO_SRC_DIR}/${proto_name}.proto
    DEPENDS ${PROTO_SRC_DIR}/${proto_name}.proto
    COMMENT "Generating protobuf/gRPC code for ${proto_name}.proto"
  )
  list(APPEND ALL_PROTO_GENERATED_SRCS ${pb_cc} ${grpc_pb_cc})
endforeach()

add_library(proto_lib STATIC ${ALL_PROTO_GENERATED_SRCS})
target_include_directories(proto_lib PUBLIC ${PROTO_GEN_DIR})
target_link_libraries(proto_lib PUBLIC
  protobuf::libprotobuf
  gRPC::grpc++
  gRPC::grpc++_reflection
)

# --- Binaries ---------------------------------------------------------------

add_subdirectory(common)
add_subdirectory(indexer)
add_subdirectory(services/retrieval)
add_subdirectory(services/controller)
add_subdirectory(services/gateway)
add_subdirectory(services/cache)
add_subdirectory(services/reranker)
