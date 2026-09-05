add_executable(fps_raycaster WIN32
    fps_raycaster.cpp
)

target_compile_features(fps_raycaster PRIVATE cxx_std_17)

if(WIN32)
    target_link_libraries(fps_raycaster PRIVATE user32 gdi32)
endif()