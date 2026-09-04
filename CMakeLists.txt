cmake_minimum_required(VERSION 3.15...4.4.3)
option(build_shared_libs "Build shared libraries" ON)
option(RAST_BUILD_SAMPLES "Build samples" ON)

set(rast_VERSION_MAJOR 0)
set(rast_VERSION_MINOR 1)
set(rast_VERSION_PATCH 0)
set (rast_VERSION "${rast_VERSION_MAJOR}.${rast_VERSION_MINOR}.${rast_VERSION_PATCH}")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)


project(rast VERSION ${rast_VERSION} LANGUAGES CXX)
    
if(MSVC)
    add_compile_options(/MP)
endif(MSVC)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/externals/CMakeLists.txt")
    add_subdirectory(externals EXCLUDE_FROM_ALL)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/graphics/CMakeLists.txt")
    add_subdirectory(graphics)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/math/CMakeLists.txt")
    add_subdirectory(math)
endif()

if(RAST_BUILD_SAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/CMakeLists.txt")
    add_subdirectory(samples)
endif()


