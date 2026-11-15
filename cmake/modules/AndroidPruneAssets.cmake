# Removes from the Android assets the files copied from res/ that are never
# used on Android (about 60 MB uncompressed). Run with:
#   cmake -DASSETS_DIR=<android-build/assets> -P AndroidPruneAssets.cmake
if(NOT ASSETS_DIR OR NOT IS_DIRECTORY "${ASSETS_DIR}")
  message(FATAL_ERROR "ASSETS_DIR is not a directory: ${ASSETS_DIR}")
endif()

# Translation sources: Qt only loads the compiled .qm files
file(GLOB _ts_files "${ASSETS_DIR}/translations/*.ts")
# Desktop packaging files
set(
  _paths
  "${ASSETS_DIR}/osx"
  "${ASSETS_DIR}/linux"
  "${ASSETS_DIR}/Mixxx-Keyboard-Shortcuts.pdf"
)
set(_count 0)
foreach(_path IN LISTS _ts_files _paths)
  if(EXISTS "${_path}")
    file(REMOVE_RECURSE "${_path}")
    math(EXPR _count "${_count} + 1")
  endif()
endforeach()
message(STATUS "Android assets: removed ${_count} unused files/folders")
