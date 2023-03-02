cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 17)

if(${GTEST})
    #Creates a variable named TEST_FILES that can be used via ${TEST_FILES}
    set(TEST_FILES
            src/test/testClass_test.cpp
            )
    #Libraries that are needed by the test files
    set(TEST_LIBS
            testClass_lib
            )

    #adds include directories before adding subdirectories so they have access to the header prototypes
    include_directories(components/testClass)

    #tells cmake that there is another CMakeList.txt in components/testClass that it needs to process
    add_subdirectory(src/test)

    project(unit-tests)

    message("running GTEST")
    add_subdirectory(googletest)

    # adds to the include path
    # We have access to the var gtest_SOURCE_DIR because it is being set in the `lib` CMake files that is called above
    # via add_subdirectory(google)
    include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
    include_directories(${gmock_SOURCE_DIR}/include ${gmock_SOURCE_DIR})

    # Creates an executable called Google_Test_run and adds tests from testClass/test/test.cpp
    add_executable(Google_Tests_run ${TEST_FILES})

    # link the project libraries and google test library to the executable Google_Tests_run
    target_link_libraries(Google_Tests_run ${TEST_LIBS} gtest gmock gtest_main)
else()
      set(EXTRA_COMPONENT_DIRS src)

      set(application_name "esp32_lora_gateway")
      set(application_version "0.1.0")

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

endif()