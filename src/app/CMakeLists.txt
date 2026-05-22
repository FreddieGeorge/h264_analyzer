set(ZSTREAMEYE_APP_SOURCES
    ../main.cpp
    AnalysisStore.h
    AnalysisStore.cpp
    DecodeSession.h
    DecodeSession.cpp
    ExportController.h
    ExportController.cpp
    MainWindow.h
    MainWindow.cpp
    UpdateChecker.h
    UpdateChecker.cpp
    ../ui/FrameListView.h
    ../ui/FrameListView.cpp
    ../ui/BitstreamHexView.h
    ../ui/BitstreamHexView.cpp
    ../ui/LogDock.h
    ../ui/LogDock.cpp
    ../ui/H264PropertyTreeBuilder.h
    ../ui/H264PropertyTreeBuilder.cpp
    ../ui/PropertyTreeView.h
    ../ui/PropertyTreeView.cpp
    ../ui/StatsDock.h
    ../ui/StatsDock.cpp
    ../ui/VideoCanvas.h
    ../ui/VideoCanvas.cpp
    ../../resources/app.qrc
)

if(WIN32)
    list(APPEND ZSTREAMEYE_APP_SOURCES ../../resources/windows/ZStreamEye.rc)
endif()

add_executable(ZStreamEyeApp ${ZSTREAMEYE_APP_SOURCES})

target_link_libraries(ZStreamEyeApp PRIVATE
    ZStreamEyeCore
    Qt6::Widgets
    Qt6::Network
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

target_compile_definitions(ZStreamEyeApp PRIVATE
    ZSTREAMEYE_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(ZStreamEyeApp PRIVATE /utf-8)
endif()

set_target_properties(ZStreamEyeApp PROPERTIES
    OUTPUT_NAME ZStreamEyeApp
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
