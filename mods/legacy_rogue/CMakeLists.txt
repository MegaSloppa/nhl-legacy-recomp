# Legacy Rogue - compiled directly into the nhllegacy target so it inherits the
# game's include paths (rex SDK, imgui, generated/), compile definitions and flags.
# Root CMakeLists.txt must call add_subdirectory(mods/legacy_rogue) AFTER the
# nhllegacy target is defined. Do NOT target_link_libraries a separate lib.
# Adding a file here needs scripts\_game_ffx_build.bat (re-configure), not the incremental build.
target_sources(nhllegacy PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/perk_catalog.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/run_state.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rogue_mod.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rogue_hooks.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rogue_panel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/rogue_probe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/tweak_index.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/reactions.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fx.cpp)
target_include_directories(nhllegacy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nhllegacy PRIVATE winmm)  # PlaySoundW in fx.cpp
