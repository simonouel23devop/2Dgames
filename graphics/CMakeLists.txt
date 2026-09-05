option(RAST_BUILD_ASSIMP_MODEL "Build the Assimp model loading module" ON)

if(NOT RAST_BUILD_ASSIMP_MODEL)
    return()
endif()

find_package(assimp CONFIG QUIET)

if(NOT assimp_FOUND)
    message(STATUS "Assimp was not found; skipping rast_graphics model loader")
    return()
endif()

add_library(rast_graphics STATIC
    src/Model.cpp
)

target_compile_features(rast_graphics PUBLIC cxx_std_17)
target_include_directories(rast_graphics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

if(TARGET assimp::assimp)
    target_link_libraries(rast_graphics PUBLIC assimp::assimp)
elseif(TARGET assimp)
    target_link_libraries(rast_graphics PUBLIC assimp)
else()
    message(FATAL_ERROR "Assimp was found, but no assimp::assimp or assimp target is available")
endif()
