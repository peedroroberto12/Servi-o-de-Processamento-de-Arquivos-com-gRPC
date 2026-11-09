cmake_minimum_required(VERSION 3.16)
project(file_processor_server CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ protobuf)
find_program(PROTOC protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../proto")
set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(GEN_SRCS
    "${GEN_DIR}/file_processor.pb.cc"
    "${GEN_DIR}/file_processor.grpc.pb.cc")

# gera o codigo c++ a partir do .proto
add_custom_command(
    OUTPUT ${GEN_SRCS}
    COMMAND ${PROTOC} --proto_path=${PROTO_DIR}
            --cpp_out=${GEN_DIR} --grpc_out=${GEN_DIR}
            --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
            ${PROTO_DIR}/file_processor.proto
    DEPENDS ${PROTO_DIR}/file_processor.proto)

add_executable(server server.cc ${GEN_SRCS})
target_include_directories(server PRIVATE ${GEN_DIR} ${GRPC_INCLUDE_DIRS})
target_link_libraries(server ${GRPC_LIBRARIES})
