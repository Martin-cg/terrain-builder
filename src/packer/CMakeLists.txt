add_library(packerlib INTERFACE)
target_include_directories(packerlib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(packerlib INTERFACE bvhbuilderlib dagbuilderlib)

add_executable(packer
    cli.cpp
    main.cpp
)
target_link_libraries(packer PRIVATE packerlib CLI11::CLI11)
