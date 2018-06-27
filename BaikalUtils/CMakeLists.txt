set(SOURCES
    cl_config_manager.cpp
    cl_config_manager.h
    vk_config_manager.cpp
    vk_config_manager.h
    config_manager.h
    )

add_library(BaikalUtils STATIC ${SOURCES})
set_property(TARGET BaikalUtils PROPERTY POSITION_INDEPENDENT_CODE ON)

target_compile_features(BaikalUtils PRIVATE cxx_std_14)
target_include_directories(BaikalUtils
    PUBLIC "${Baikal_SOURCE_DIR}/BaikalUtils"
    PUBLIC $<TARGET_PROPERTY:Baikal,INCLUDE_DIRECTORIES>
    )
target_link_libraries(BaikalUtils PRIVATE GLEW::GLEW)
