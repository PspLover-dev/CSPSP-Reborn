cmake_minimum_required(VERSION 3.10)
project(CSPSP CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC
    src/main.cpp
    src/App.cpp
    src/Input.cpp
    src/Assets.cpp
    src/Map.cpp
    src/World.cpp
    src/Net.cpp
)

add_executable(cspsp ${SRC})
target_include_directories(cspsp PRIVATE src)

find_package(SDL2 REQUIRED)
target_include_directories(cspsp PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(cspsp PRIVATE ${SDL2_LIBRARIES})

if(PSP)
    target_link_libraries(cspsp PRIVATE
        SDL2_image
        SDL2_ttf
        harfbuzz
        png16
        jpeg
        freetype
        bz2
        z
        m
        pspnet
        pspnet_adhoc
        pspnet_adhocctl
        pspnet_adhocmatching
        pspwlan
        psputility
        pspctrl
    )
    create_pbp_file(
        TARGET cspsp
        TITLE "CS 2D PSP"
        ICON_PATH ${CMAKE_SOURCE_DIR}/Gfx/ICON0.png
        BACKGROUND_PATH ${CMAKE_SOURCE_DIR}/Gfx/PIC1.png
        PREVIEW_PATH NULL
        VERSION "1.00"
    )
    add_custom_command(TARGET cspsp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:cspsp>/Gfx $<TARGET_FILE_DIR:cspsp>/maps
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Gfx $<TARGET_FILE_DIR:cspsp>/Gfx
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/maps $<TARGET_FILE_DIR:cspsp>/maps
        COMMENT "Copying Gfx and maps next to EBOOT.PBP"
    )
else()
    find_package(SDL2_image REQUIRED)
    find_package(SDL2_ttf REQUIRED)
    target_link_libraries(cspsp PRIVATE SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf)
endif()
