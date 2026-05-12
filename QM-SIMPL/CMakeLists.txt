set(APP "QM-SIMPL")

add_executable(${APP}
    src/Main.cpp
)

file(GLOB_RECURSE
    APP_SOURCES 
        "src/*.cpp" 
        "src/*.hpp"
)

target_sources(${APP}
    PRIVATE
        ${APP_SOURCES}
        ${IMGUI_SOURCES}
)

target_include_directories(${APP}
    PRIVATE
        src
)

target_link_libraries(${APP}
    PRIVATE
        OpenGLFramework
)

add_custom_command(TARGET ${APP} POST_BUILD
    COMMAND 
        ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/${APP}/assets
        $<TARGET_FILE_DIR:${APP}>/assets
)