if(${GTEST})
    project(testClass)

    #Creates an env var for building HEADER_FILES
    set(HEADER_FILES
            testClass.h
            )
    #Creates an env var for building SOURCE_FILES
    set(SOURCE_FILES
            testClass.cpp
            )

    #Adds a library called testClass_lib to be built from the SOURCE_FILES, will be statically linked
    add_library(testClass_lib STATIC ${SOURCE_FILES} ${HEADER_FILES})
else()
    set(SOURCE_FILES
            testClass.cpp
            )

    set(COMPONENT_SRCS "${SOURCE_FILES}")
    set(COMPONENT_ADD_INCLUDEDIRS ".")
    register_component()
endif()