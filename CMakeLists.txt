cmake_minimum_required (VERSION 3.28)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (pixel_engine::app)

file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")
file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cpp")

if (NOT boost_pfr) 
  add_subdirectory(libs/pfr)
endif()

add_library(app ${HEADERS} ${SOURCES})
target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(app boost_pfr)

set_property(TARGET app PROPERTY CXX_STANDARD 20)
