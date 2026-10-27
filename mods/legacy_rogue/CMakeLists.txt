# Legacy Rogue - add to the game target from the repo's root CMakeLists.txt:
#   add_subdirectory(mods/legacy_rogue)
#   target_link_libraries(nhllegacy PRIVATE legacy_rogue)   # or target_sources
# Hooks override generated sub_XXXXXXXX symbols, so these sources must link into
# the same image as generated/default (an OBJECT library keeps that true).
add_library(legacy_rogue OBJECT
  src/perk_catalog.cpp
  src/run_state.cpp
  src/rogue_mod.cpp
  src/rogue_hooks.cpp
  src/rogue_panel.cpp
  src/reactions.cpp
  src/fx.cpp)
target_include_directories(legacy_rogue PUBLIC include ${CMAKE_SOURCE_DIR})
# Inherit the game's include dirs / rexglue SDK / imgui the same way src/ does.
# TODO: mirror whatever the nhllegacy target uses (rex::runtime, imgui target).
# fx.cpp uses PlaySoundW
target_link_libraries(legacy_rogue PRIVATE winmm)
