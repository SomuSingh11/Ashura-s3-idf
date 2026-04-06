SET(PROJECT_VER "1.0")
cmake_minimum_required(VERSION 3.16.0)

# Tell IDF to treat 'src' as an extra component
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ashura-core-s3-idf)
