cmake_minimum_required(VERSION 3.15)
project(OnlabSzakdoga)

# set(APP "GS-PIA_2D")
# set(APP "GS-PIA_3D")
set(APP "QM-SIMPL")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    OpenGLFramework
    GIT_REPOSITORY https://github.com/mandliors/OpenGLFramework.git
    GIT_TAG master
)
FetchContent_MakeAvailable(OpenGLFramework)

add_subdirectory(${APP})
