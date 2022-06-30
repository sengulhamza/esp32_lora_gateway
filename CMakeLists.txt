cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 17)

set(EXTRA_COMPONENT_DIRS src)

set(application_name "esp32_lora_gateway")
set(application_version "0.0.1")

set(PROJECT_VER ${application_version})

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# idf.py -DCMAKE_BUILD_TYPE=Debug reconfigure
# idf.py -DCMAKE_BUILD_TYPE=Release reconfigure
if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(opt_level -O0)
  add_definitions(-DDEBUG_BUILD)
else()
  set(opt_level -Os)
endif()

project(${application_name})
