cmake_minimum_required(VERSION 3.22.1)
project("chickenmod")

# Реальные пути из reveny репо
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui/app/src/main/jni/Include/ImGui)
set(DOBBY_DIR ${CMAKE_SOURCE_DIR}/imgui/app/src/main/jni/Include/Dobby)

add_library(chickenmod SHARED
    main.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_android.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Dobby уже есть в репо!
add_library(dobby STATIC IMPORTED)
set_target_properties(dobby PROPERTIES
    IMPORTED_LOCATION
    ${DOBBY_DIR}/${ANDROID_ABI}/libdobby.a
)

target_include_directories(chickenmod PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${DOBBY_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_compile_options(chickenmod PRIVATE
    -std=c++17
    -O2
    -fvisibility=hidden
)

target_link_libraries(chickenmod
    dobby
    android
    log
    EGL
    GLESv2
)
