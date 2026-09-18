add_library(packerlib
    texture/basis.cpp
)
target_include_directories(packerlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(packerlib PUBLIC bvhbuilderlib dagbuilderlib PRIVATE basisu_encoder)

add_executable(packer
    cli.cpp
    main.cpp
)
target_link_libraries(packer PRIVATE packerlib CLI11::CLI11)
