cmake_minimum_required(VERSION 3.22)
project(tmc C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

get_filename_component(PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)

add_definitions(-DPC_PORT -DNON_MATCHING -DUSA -DENGLISH -DREVISION=0)
add_definitions(-DANDROID_PORT -DTMC_PORT_VERSION="\"0.1.3\"" -DMODE1_GBA_WIDTH=240)
add_definitions(-DFMT_HEADER_ONLY=1)

include_directories(
    ${PROJ_ROOT}/include
    ${PROJ_ROOT}/port
    ${PROJ_ROOT}/libs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viruappu_stub
    ${CMAKE_CURRENT_SOURCE_DIR}/src/virtuaapu_stub
    ${PROJ_ROOT}/libs/agbplay_core
    ${PROJ_ROOT}/tools/src/assets_extractor
    ${PROJ_ROOT}/build/USA
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SDL3
)

add_library(viruappu STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/viruappu_stub/virtuappu.c
)
target_compile_definitions(viruappu PRIVATE MODE1_GBA_WIDTH=240)

# VirtuaAPU stub (not needed for audio - agbplay handles it)
add_library(virtuaapu STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/virtuaapu_stub/virtuaapu.c
)

file(GLOB AGBPLAY_SRCS ${PROJ_ROOT}/libs/agbplay_core/*.cpp)
add_library(agbplay STATIC ${AGBPLAY_SRCS})

set(GAME_SRCS
    ${PROJ_ROOT}/src/main.c
    ${PROJ_ROOT}/src/common.c
    ${PROJ_ROOT}/src/interrupts.c
    ${PROJ_ROOT}/src/game.c
    ${PROJ_ROOT}/src/title.c
    ${PROJ_ROOT}/src/fileselect.c
    ${PROJ_ROOT}/src/flags.c
    ${PROJ_ROOT}/src/save.c
    ${PROJ_ROOT}/src/fade.c
    ${PROJ_ROOT}/src/color.c
    ${PROJ_ROOT}/src/vram.c
    ${PROJ_ROOT}/src/screenTileMap.c
    ${PROJ_ROOT}/src/message.c
    ${PROJ_ROOT}/src/text.c
    ${PROJ_ROOT}/src/sound.c
    ${PROJ_ROOT}/src/script.c
    ${PROJ_ROOT}/src/scroll.c
    ${PROJ_ROOT}/src/room.c
    ${PROJ_ROOT}/src/roomInit.c
    ${PROJ_ROOT}/src/entity.c
    ${PROJ_ROOT}/src/physics.c
    ${PROJ_ROOT}/src/collision.c
    ${PROJ_ROOT}/src/movement.c
    ${PROJ_ROOT}/src/affine.c
    ${PROJ_ROOT}/src/sineTable.c
    ${PROJ_ROOT}/src/ui.c
    ${PROJ_ROOT}/src/player.c
    ${PROJ_ROOT}/src/playerUtils.c
    ${PROJ_ROOT}/src/playerHitbox.c
    ${PROJ_ROOT}/src/playerItem.c
    ${PROJ_ROOT}/src/playerItemUtils.c
    ${PROJ_ROOT}/src/playerItemDefinitions.c
    ${PROJ_ROOT}/src/enemy.c
    ${PROJ_ROOT}/src/enemyUtils.c
    ${PROJ_ROOT}/src/npc.c
    ${PROJ_ROOT}/src/npcUtils.c
    ${PROJ_ROOT}/src/npcFunctions.c
    ${PROJ_ROOT}/src/npcDefinitions.c
    ${PROJ_ROOT}/src/object.c
    ${PROJ_ROOT}/src/objectUtils.c
    ${PROJ_ROOT}/src/objectDefinitions.c
    ${PROJ_ROOT}/src/projectile.c
    ${PROJ_ROOT}/src/projectileUtils.c
    ${PROJ_ROOT}/src/manager.c
    ${PROJ_ROOT}/src/item.c
    ${PROJ_ROOT}/src/itemUtils.c
    ${PROJ_ROOT}/src/itemDefinitions.c
    ${PROJ_ROOT}/src/itemMetaData.c
    ${PROJ_ROOT}/src/subtask.c
    ${PROJ_ROOT}/src/cutscene.c
    ${PROJ_ROOT}/src/backgroundAnimations.c
    ${PROJ_ROOT}/src/beanstalkSubtask.c
    ${PROJ_ROOT}/src/enterPortalSubtask.c
    ${PROJ_ROOT}/src/gameOverTask.c
    ${PROJ_ROOT}/src/gameUtils.c
    ${PROJ_ROOT}/src/gameData.c
    ${PROJ_ROOT}/src/kinstone.c
    ${PROJ_ROOT}/src/droptables.c
    ${PROJ_ROOT}/src/demo.c
    ${PROJ_ROOT}/src/debug.c
    ${PROJ_ROOT}/src/flagDebug.c
    ${PROJ_ROOT}/src/staffroll.c
    ${PROJ_ROOT}/src/code_08049CD4.c
    ${PROJ_ROOT}/src/code_08049DF4.c
    ${PROJ_ROOT}/src/code_0805EC04.c
)

file(GLOB PLAYER_ITEM_SRCS ${PROJ_ROOT}/src/playerItem/*.c)
file(GLOB ENEMY_SRCS       ${PROJ_ROOT}/src/enemy/*.c)
file(GLOB NPC_SRCS         ${PROJ_ROOT}/src/npc/*.c)
file(GLOB OBJECT_SRCS      ${PROJ_ROOT}/src/object/*.c)
file(GLOB PROJ_SRCS        ${PROJ_ROOT}/src/projectile/*.c)
file(GLOB MANAGER_SRCS     ${PROJ_ROOT}/src/manager/*.c)
file(GLOB ITEM_SRCS        ${PROJ_ROOT}/src/item/*.c)
file(GLOB SUBTASK_SRCS     ${PROJ_ROOT}/src/subtask/*.c)
file(GLOB MENU_SRCS        ${PROJ_ROOT}/src/menu/*.c)
file(GLOB DATA_SRCS        ${PROJ_ROOT}/src/data/*.c)
file(GLOB WORLDEVENT_SRCS  ${PROJ_ROOT}/src/worldEvent/*.c)

set(PORT_REUSE_SRCS
    ${PROJ_ROOT}/port/port_draw.c
    ${PROJ_ROOT}/port/port_animation.c
    ${PROJ_ROOT}/port/port_gba_mem.c
    ${PROJ_ROOT}/port/port_bios.c
    ${PROJ_ROOT}/port/port_rom.c
    ${PROJ_ROOT}/port/port_math.c
    ${PROJ_ROOT}/port/port_text_render.c
    ${PROJ_ROOT}/port/port_stubs.c
    ${PROJ_ROOT}/port/stubs_autogen.c
    ${PROJ_ROOT}/port/data_stubs_autogen.c
    ${PROJ_ROOT}/port/data_const_stubs.c
    ${PROJ_ROOT}/port/port_rom_tables.c
    ${PROJ_ROOT}/port/port_linked_stubs.c
    ${PROJ_ROOT}/port/port_figurines.c
    ${PROJ_ROOT}/port/port_hdma.c
    ${PROJ_ROOT}/port/port_upscale.c
    ${PROJ_ROOT}/port/port_save.c
    ${PROJ_ROOT}/port/port_softslots.c
    ${PROJ_ROOT}/port/port_filter.c
    ${PROJ_ROOT}/port/port_gameplay_stubs.c
    ${PROJ_ROOT}/port/port_m4a_stubs.c
    ${PROJ_ROOT}/port/port_room_funcs.c
    ${PROJ_ROOT}/port/port_script_funcs.c
    ${PROJ_ROOT}/port/port_inline_ptrs.c
    ${PROJ_ROOT}/port/port_debug_actions.c
    ${PROJ_ROOT}/port/port_m4a_backend.cpp
    ${PROJ_ROOT}/port/port_asset_index.c
    ${PROJ_ROOT}/port/port_asset_log.cpp
    ${PROJ_ROOT}/port/port_asset_pipeline.cpp
    ${PROJ_ROOT}/port/port_asset_loader.cpp
    ${PROJ_ROOT}/port/port_asset_pak_loader.cpp
    ${PROJ_ROOT}/port/port_debug_menu.cpp
    ${PROJ_ROOT}/port/generated_sounds_embed.cpp
)

set(PORT_ANDROID_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port_android_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port_android_render.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port_android_ppu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port_android_audio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port_android_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port_android_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port_android_rom.c
)

add_library(tmc SHARED
    ${GAME_SRCS}
    ${PLAYER_ITEM_SRCS}
    ${ENEMY_SRCS}
    ${NPC_SRCS}
    ${OBJECT_SRCS}
    ${PROJ_SRCS}
    ${MANAGER_SRCS}
    ${ITEM_SRCS}
    ${SUBTASK_SRCS}
    ${MENU_SRCS}
    ${DATA_SRCS}
    ${WORLDEVENT_SRCS}
    ${PORT_REUSE_SRCS}
    ${PORT_ANDROID_SRCS}
)

target_link_libraries(tmc
    viruappu
    virtuaapu
    agbplay
    EGL
    GLESv3
    OpenSLES
    android
    log
    z
    m
)

target_compile_options(tmc PRIVATE
    -Wall -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-address-of-packed-member
    -fno-strict-aliasing
    -fwrapv
    -fvisibility=default
    -Wno-incompatible-pointer-types
    -Wno-return-type
    -Wno-implicit-function-declaration
    -Wno-int-to-void-pointer-cast
    -Wno-void-pointer-to-int-cast
    -Wno-pointer-to-int-cast
    -Wno-unused-function
    -Wno-unused-variable
)
