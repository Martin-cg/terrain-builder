add_library(bvhbuilderlib INTERFACE)
target_include_directories(bvhbuilderlib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bvhbuilderlib INTERFACE dagbuilderlib)

if(ALP_BUILD_BVH_BUILDER)
    add_executable(bvh-builder
        cli.cpp
        main.cpp
    )
    target_link_libraries(bvh-builder PRIVATE bvhbuilderlib CLI11::CLI11)
endif()
