cmake_minimum_required(VERSION 3.16)
project(HaigakuManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Set the source directory for resources
set(RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets)

# Define source files with new paths
set(PROJECT_SOURCES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/StatisticsDialog.cpp
    src/ui/StatisticsDialog.h
    src/models/ThumbnailListModel.cpp
    src/models/ThumbnailListModel.h
    src/services/ThumbnailLoader.cpp
    src/services/ThumbnailLoader.h
    src/services/ThumbnailWorker.cpp
    src/services/ThumbnailWorker.h
    ${RESOURCE_DIR}/resources.qrc
)

# Add include directories for subfolders in src
include_directories(
    src/ui
    src/models
    src/services
)

qt_add_executable(HaigakuManager
    WIN32 # Explicitly mark as a Windows (GUI) executable, no console
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

# Source groups for IDE organization
set(SRC_FILES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/StatisticsDialog.cpp
    src/ui/StatisticsDialog.h
    src/models/ThumbnailListModel.cpp
    src/models/ThumbnailListModel.h
    src/services/ThumbnailLoader.cpp
    src/services/ThumbnailLoader.h
    src/services/ThumbnailWorker.cpp
    src/services/ThumbnailWorker.h
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source Files" FILES ${SRC_FILES})
source_group("Resources" FILES ${RESOURCE_DIR}/resources.qrc ${RESOURCE_DIR}/aero_style.qss)


target_link_libraries(HaigakuManager PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets)

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
