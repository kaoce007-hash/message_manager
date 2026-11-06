cmake_minimum_required(VERSION 3.16)
project(message_manager VERSION 18.0 LANGUAGES CXX)

add_library(message_manager STATIC
    src/MessageCommon.cpp src/DumpTable.cpp src/EnumDefinitions.cpp
    src/BinaryMessage.cpp src/MessageInterface.cpp src/MessageManager.cpp)
add_library(binary_message ALIAS message_manager)
add_library(message_manager::binary_message ALIAS message_manager)
add_library(message_manager::message_manager ALIAS message_manager)
target_compile_features(message_manager PUBLIC cxx_std_17)
target_include_directories(message_manager PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>
    $<INSTALL_INTERFACE:include>)
if(MSVC)
    target_compile_options(message_manager PUBLIC /utf-8)
endif()
set_target_properties(message_manager PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(simple_demo examples/simple_main.cpp)
target_link_libraries(simple_demo PRIVATE binary_message)

add_executable(demo examples/main.cpp)
target_link_libraries(demo PRIVATE binary_message)
file(COPY json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(test)
endif()

install(TARGETS message_manager EXPORT message_managerTargets ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY third_party/nlohmann DESTINATION include)
install(FILES third_party/LICENSE.nlohmann-json.MIT DESTINATION share/licenses/message_manager)
install(EXPORT message_managerTargets NAMESPACE message_manager:: DESTINATION lib/cmake/message_manager)
