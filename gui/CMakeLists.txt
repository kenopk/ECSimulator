find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(GUI_SOURCES
    main.cpp

    app/MainWindow.h        app/MainWindow.cpp
    app/EditorWindow.h      app/EditorWindow.cpp

    canvas/CircuitScene.h   canvas/CircuitScene.cpp
    canvas/CircuitView.h    canvas/CircuitView.cpp

    commands/Commands.h     commands/Commands.cpp

    dialogs/ElementDialog.h  dialogs/ElementDialog.cpp
    dialogs/SettingsDialog.h dialogs/SettingsDialog.cpp
    dialogs/WireNoteDialog.h dialogs/WireNoteDialog.cpp

    items/CircuitItem.h     items/CircuitItem.cpp
    items/NoteItem.h        items/NoteItem.cpp
    items/WireNote.h
    items/SymbolPainter.h   items/SymbolPainter.cpp   items/SymbolPainterExtra.inc

    pcb/PcbCore.h
    pcb/PcbContext.h
    pcb/PcbWidgets.h        pcb/PcbWidgets.cpp
    pcb/PcbPage.h           pcb/PcbPage.cpp
    pcb/PcbPagesInternal.h  pcb/PcbPages.cpp
    pcb/PcbPagesLines.cpp   pcb/PcbPagesCurrent.cpp
    pcb/PcbPagesBoard.cpp   pcb/PcbPagesCircuits.cpp
    pcb/PcbDialogs.h        pcb/PcbDialogs.cpp
    pcb/PcbHelp.h           pcb/PcbHelp.cpp
    pcb/PcbTrace.h
    pcb/PcbOptions.h        pcb/PcbOptions.cpp
    pcb/PcbToolkitSection.h pcb/PcbToolkitSection.cpp

    project/ProjectFile.h   project/ProjectFile.cpp
    project/ProjectPanel.h  project/ProjectPanel.cpp

    scope/ScopeDialog.h     scope/ScopeDialog.cpp
    scope/ScopeMenu.h       scope/ScopeMenu.cpp
    scope/ScopeSource.h     scope/ScopeSource.cpp
    scope/ScopePlot.h       scope/ScopePlot.cpp
    scope/ScopeWidget.h     scope/ScopeWidget.cpp
    scope/ScopeItem.h       scope/ScopeItem.cpp

    sim/SimIssue.h
    sim/Preflight.h         sim/Preflight.cpp
    sim/IssueLog.h          sim/IssueLog.cpp
    sim/IssueBanner.h       sim/IssueBanner.cpp

    theme/Theme.h           theme/Theme.cpp

    util/CoreTypes.h
    util/Units.h            util/Units.cpp

    tools/KarnaughLinks.h   tools/KarnaughLinks.cpp
    tools/KarnaughSection.h tools/KarnaughSection.cpp
    tools/KarnaughSolver.h  tools/KarnaughSolver.cpp
    tools/KarnaughWidget.h  tools/KarnaughWidget.cpp

    widgets/DetachableTabBar.h widgets/DetachableTabBar.cpp
    widgets/ParamForm.h       widgets/ParamForm.cpp
    widgets/PropertiesPanel.h widgets/PropertiesPanel.cpp
    widgets/SimulationPanel.h widgets/SimulationPanel.cpp

    resources/resources.qrc
)

if(WIN32)
    list(APPEND GUI_SOURCES resources/icon.rc)
endif()

qt_add_executable(GOVNO ${GUI_SOURCES})

target_include_directories(GOVNO PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/resources   # для icon.rc (путь images/logo.ico)
)

target_link_libraries(GOVNO PRIVATE
    Qt6::Widgets
    Qt6::PrintSupport
    govno::core
)

set_target_properties(GOVNO PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS GOVNO
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
