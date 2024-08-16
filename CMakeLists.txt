cmake_minimum_required (VERSION 3.28)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (pixel_engine_app)

file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")
file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cpp")

if (NOT TARGET boost_pfr)
  add_subdirectory(libs/pfr)
endif()

if (NOT TARGET EnTT::EnTT)
  add_subdirectory(libs/entt)
endif()

if (NOT TARGET spdlog)
  add_subdirectory(libs/spdlog)
endif()

if (NOT TARGET BSThreadPool)
  add_subdirectory(libs/thread-pool)
endif()

add_library(pixel_engine_app ${HEADERS} ${SOURCES})
target_include_directories(pixel_engine_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(pixel_engine_app boost_pfr)
target_link_libraries(pixel_engine_app EnTT::EnTT)
target_link_libraries(pixel_engine_app spdlog::spdlog)
target_link_libraries(pixel_engine_app BSThreadPool)

set_property(TARGET pixel_engine_app PROPERTY CXX_STANDARD 20)

add_library(pixel_engine::app ALIAS pixel_engine_app)
