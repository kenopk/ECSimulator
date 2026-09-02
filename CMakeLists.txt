cmake_minimum_required(VERSION 3.16)

project(GOVNO VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# Добавь после project() или перед add_executable():
# add_compile_definitions(DEBUG)  # DEBUG: Определяем макрос DEBUG для сборки

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        resources.qrc
        icon.rc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(GOVNO
        ${PROJECT_SOURCES}
        editorwindow.h editorwindow.cpp
        pin.h pin.cpp
        component.h component.cpp
        infinitegraphicsscene.h infinitegraphicsscene.cpp
        infinitegraphicsview.h infinitegraphicsview.cpp
        wire.h wire.cpp

    )
else()
    if(ANDROID)
        add_library(GOVNO SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(GOVNO
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(GOVNO PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.GOVNO)
endif()

set_target_properties(GOVNO PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS GOVNO
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)