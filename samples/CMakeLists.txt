if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fps_raycaster.cpp")
    add_executable(fps_raycaster WIN32
        fps_raycaster.cpp
    )

    target_compile_features(fps_raycaster PRIVATE cxx_std_17)

    if(WIN32)
        target_link_libraries(fps_raycaster PRIVATE user32 gdi32)
    endif()
endif()

add_executable(fps_3d_opengl WIN32
    fps_3d_opengl.cpp
)

target_compile_features(fps_3d_opengl PRIVATE cxx_std_17)

if(WIN32)
    target_link_libraries(fps_3d_opengl PRIVATE user32 gdi32 opengl32 glu32)
endif()