set(APPLICATION_SOURCES
    Application/application.cpp
    Application/application.h
    Application/vk_application.cpp
    Application/vk_application.h
    Application/app_utils.cpp
    Application/app_utils.h
    Application/vk_render.cpp
    Application/vk_render.h
    Application/render.cpp
    Application/render.h
    Application/render.h)

set(MAIN_SOURCES main.cpp)

set(KERNEL_SOURCES
    Kernels/GLSL/simple.fsh
    Kernels/GLSL/simple.vsh)

set(SOURCES
    ${APPLICATION_SOURCES}
    ${MAIN_SOURCES}
    ${KERNEL_SOURCES})

source_group("Application" FILES ${APPLICATION_SOURCES})
source_group("ImGUI" FILES ${IMGUI_SORUCES})
source_group("Utils" FILES ${UTILS_SOURCES})
source_group("Kernels\\GLSL" FILES ${KERNEL_SOURCES})

add_executable(BaikalStandaloneRPR ${SOURCES})
target_compile_features(BaikalStandaloneRPR PRIVATE cxx_std_14)
#Add project root since BaikalStandaloneRPR directly includes Baikal/* files
target_include_directories(BaikalStandaloneRPR
    PRIVATE ${Baikal_SOURCE_DIR}
    PRIVATE .
    PRIVATE ${Baikal_SOURCE_DIR}/RadeonRays/RadeonRays/include
    )
target_link_libraries(BaikalStandaloneRPR PRIVATE RadeonProRender64 glfw3::glfw3 OpenGL::GL GLEW::GLEW VKW )

set_target_properties(BaikalStandaloneRPR
    PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${Baikal_SOURCE_DIR}/BaikalStandaloneRPR)

add_custom_target(BaikalStandaloneRPRKernelsDir)

set(BaikalStandaloneRPR_KERNELS_DIR BaikalStandaloneRPR/Kernels)
FILE(TO_NATIVE_PATH ${Baikal_SOURCE_DIR}/${BaikalStandaloneRPR_KERNELS_DIR} BaikalStandaloneRPR_KERNELS_SRC)
FILE(TO_NATIVE_PATH ${CMAKE_BINARY_DIR}/${BaikalStandaloneRPR_KERNELS_DIR} BaikalStandaloneRPR_KERNELS_DST)

if (WIN32)
    add_custom_command(TARGET BaikalStandaloneRPRKernelsDir POST_BUILD
        COMMAND IF NOT EXIST ${BaikalStandaloneRPR_KERNELS_DST} mklink /D ${BaikalStandaloneRPR_KERNELS_DST} ${BaikalStandaloneRPR_KERNELS_SRC}
        DEPENDS ${BaikalStandaloneRPR_KERNELS_DST}
    )
else ()
    add_custom_command(TARGET BaikalStandaloneRPRKernelsDir POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${BaikalStandaloneRPR_KERNELS_SRC} ${BaikalStandaloneRPR_KERNELS_DST}
        DEPENDS ${BaikalStandaloneRPR_KERNELS_DST}
    )
endif ()

add_dependencies(BaikalStandaloneRPR ResourcesDir BaikalKernelsDir BaikalStandaloneRPRKernelsDir)

if (WIN32)
    add_custom_command(TARGET BaikalStandaloneRPR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${BaikalStandaloneRPR_DLLS}
            "$<TARGET_FILE_DIR:BaikalStandaloneRPR>"
    )
endif ()

install(TARGETS BaikalStandaloneRPR RUNTIME DESTINATION bin)
if (WIN32)
    install(FILES ${BaikalStandaloneRPR_DLLS} DESTINATION bin)
endif ()
install(DIRECTORY ${RESOURCES_SRC} DESTINATION .)
install(DIRECTORY ${BAIKAL_KERNELS_SRC} DESTINATION Baikal)
install(DIRECTORY ${BaikalStandaloneRPR_KERNELS_SRC} DESTINATION BaikalStandaloneRPR)

#TODO: the following should be added to CMakeLists.txt of RadeonRays
#if (WIN32)
#    install(TARGETS RadeonRays RUNTIME DESTINATION bin)
#else ()
#    install(TARGETS RadeonRays LIBRARY DESTINATION lib)
#endif ()

#TODO: the following should be removed after the above being add to CMakeLists.txt of RadeonRays
if (WIN32)
    install(FILES $<TARGET_FILE_DIR:BaikalStandaloneRPR>/RadeonRays.dll DESTINATION bin)
else ()
    file(GLOB_RECURSE RADEONRAYS_SO ${CMAKE_BINARY_DIR}/libRadeonRays.*)
    install(FILES ${RADEONRAYS_SO} DESTINATION lib)
endif ()

