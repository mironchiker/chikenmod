cmake_minimum_required(VERSION 3.10.2)
project(chickenmod)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/app/src/main/jni/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/app/src/main/jni/Include/ImGui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/app/src/main/jni/Include/ImGui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/app/src/main/jni/Include/Dobby
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/app/src/main/jni/Include/KittyMemory
)

add_library(chickenmod SHARED
    main.cpp
    imgui/app/src/main/jni/Include/ImGui/imgui.cpp
    imgui/app/src/main/jni/Include/ImGui/imgui_draw.cpp
    imgui/app/src/main/jni/Include/ImGui/imgui_tables.cpp
    imgui/app/src/main/jni/Include/ImGui/imgui_widgets.cpp
    imgui/app/src/main/jni/Include/ImGui/backends/imgui_impl_android.cpp
    imgui/app/src/main/jni/Include/ImGui/backends/imgui_impl_opengl3.cpp
)

set(DOBBY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui/app/src/main/jni/Include/Dobby)
set(DOBBY_LIB ${DOBBY_DIR}/${ANDROID_ABI}/libdobby.a)

target_link_libraries(chickenmod
    PRIVATE
    ${DOBBY_LIB}
    log
    EGL
    GLESv3
)
