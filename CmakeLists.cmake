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

add_subdirectory(externals EXCLUDE_FROM_ALL)
add_subdirectory(graphics)
add_subdirectory(math)

if(RAST_BUILD_SAMPLES)
    add_subdirectory(samples)
endif(RAST_BUILD_SAMPLES)


