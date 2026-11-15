# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-s3-can-2ch)

# Make the application's OD.h visible to the canopennode component
# (required while CONFIG_CO_MULTIPLE_OD is disabled).
idf_component_get_property(canopennode_lib espressif__canopennode COMPONENT_LIB)
target_include_directories(${canopennode_lib} PRIVATE ${CMAKE_SOURCE_DIR}/main)
