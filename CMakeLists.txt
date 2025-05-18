cmake_minimum_required(VERSION 3.16)
project(HaigakuManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Set the source directory for resources
set(RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Concurrent Network) # Added Network

# ONNX Runtime paths
set(ONNXRUNTIME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/onnxruntime)
set(ONNXRUNTIME_INCLUDE_DIR ${ONNXRUNTIME_DIR}/include)
set(ONNXRUNTIME_LIB_DIR ${ONNXRUNTIME_DIR}/lib) # Assuming onnxruntime.lib and onnxruntime.dll are here

# Define source files with new paths
set(PROJECT_SOURCES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/StatisticsDialog.cpp
    src/ui/StatisticsDialog.h
    src/ui/AutoCaptionSettingsPanel.cpp     # Renamed
    src/ui/AutoCaptionSettingsPanel.h       # Renamed
    src/ui/AutoCaptionSettingsDialog.cpp 
    src/ui/AutoCaptionSettingsDialog.h   
    src/models/ThumbnailListModel.cpp
    src/models/ThumbnailListModel.h
    src/services/ThumbnailLoader.cpp
    src/services/ThumbnailLoader.h
    src/services/ThumbnailWorker.cpp
    src/services/ThumbnailWorker.h
    src/services/AutoCaptionManager.cpp 
    src/services/AutoCaptionManager.h   
    src/services/WdVIT_TaggerEngine.cpp # Added
    src/services/WdVIT_TaggerEngine.h   # Added
    ${RESOURCE_DIR}/resources.qrc
)

# Add include directories for subfolders in src
include_directories(
    src # For main.cpp to find ui/MainWindow.h etc.
    src/ui
    src/models
    src/services
    ${ONNXRUNTIME_INCLUDE_DIR} 
)
# If Qt6_Concurrent_INCLUDE_DIRS is set by find_package, this might help.
if(Qt6_Concurrent_INCLUDE_DIRS)
    include_directories(${Qt6_Concurrent_INCLUDE_DIRS})
endif()


qt_add_executable(HaigakuManager
    # WIN32 # Temporarily remove WIN32 to get console output for debugging
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

# Link against ONNX Runtime
target_link_libraries(HaigakuManager PRIVATE 
    Qt6::Core 
    Qt6::Gui 
    Qt6::Widgets 
    Qt6::Multimedia 
    Qt6::MultimediaWidgets
    Qt6::Concurrent 
    Qt6::Network # Added Network
    "${ONNXRUNTIME_LIB_DIR}/onnxruntime.lib"
)

# Copy ONNX Runtime DLL to output directory after build
add_custom_command(TARGET HaigakuManager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${ONNXRUNTIME_LIB_DIR}/onnxruntime.dll" # Assuming DLL is in lib dir
    $<TARGET_FILE_DIR:HaigakuManager>
    COMMENT "Copying onnxruntime.dll to output directory"
)

# Copy the models directory to the build output directory
set(MODEL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/models)
set(MODEL_BASE_DESTINATION_DIR $<TARGET_FILE_DIR:HaigakuManager>/models)

# Ensure the base 'models' directory exists in the build output
add_custom_command(TARGET HaigakuManager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MODEL_BASE_DESTINATION_DIR}"
    COMMENT "Ensuring models directory exists in output"
)

add_custom_command(TARGET HaigakuManager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory # Changed from copy_directory_if_different
    "${MODEL_SOURCE_DIR}/wd-vit-tagger-v3" 
    "${MODEL_BASE_DESTINATION_DIR}/wd-vit-tagger-v3" 
    COMMENT "Copying wd-vit-tagger-v3 models to output directory"
)
# If you have other models, add similar commands or a loop.

# Source groups for IDE organization
set(SRC_FILES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/StatisticsDialog.cpp
    src/ui/StatisticsDialog.h
    src/ui/AutoCaptionSettingsPanel.cpp     # Renamed
    src/ui/AutoCaptionSettingsPanel.h       # Renamed
    src/ui/AutoCaptionSettingsDialog.cpp 
    src/ui/AutoCaptionSettingsDialog.h   
    src/models/ThumbnailListModel.cpp
    src/models/ThumbnailListModel.h
    src/services/ThumbnailLoader.cpp
    src/services/ThumbnailLoader.h
    src/services/ThumbnailWorker.cpp
    src/services/ThumbnailWorker.h
    src/services/AutoCaptionManager.cpp 
    src/services/AutoCaptionManager.h   
    src/services/WdVIT_TaggerEngine.cpp # Added
    src/services/WdVIT_TaggerEngine.h   # Added
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${SRC_FILES})
source_group("Resources" FILES ${RESOURCE_DIR}/resources.qrc ${RESOURCE_DIR}/aero_style.qss)

# target_link_libraries for Qt6 components is already handled above where ONNX is linked.
# Remove this redundant one.
# target_link_libraries(HaigakuManager PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets)

# For MSVC, add option to use UTF-8 for source and execution character sets
if(MSVC)
  target_compile_options(HaigakuManager PRIVATE /utf-8)
endif()

install(TARGETS HaigakuManager
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Ensure aero_style.qss is accessible if not embedded via QRC in a way that needs it at runtime path
# However, our resources.qrc embeds it, so this might not be strictly necessary for runtime
# but good for ensuring it's part of the install if needed separately.
# install(FILES ${RESOURCE_DIR}/aero_style.qss DESTINATION .)
