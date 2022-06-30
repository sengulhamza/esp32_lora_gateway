set(sources
    src/app_mngr.cpp
    src/lora_manager.c
)

idf_component_register(
    SRCS ${sources}
    INCLUDE_DIRS . inc inc/app
    REQUIRES freertos
            esp_system
            nvs_flash
            app_update
            core
            json
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)

if (GCOV_BUILD)
MESSAGE(STATUS "Gcov build enabled for app component")
set_source_files_properties(
    ${sources}
    PROPERTIES COMPILE_FLAGS
    --coverage
)
endif()
