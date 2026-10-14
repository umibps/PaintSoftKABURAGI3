cmake_minimum_required(VERSION 3.20)
project(PaintSoftKABURAGI VERSION 3.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8)
else()
    add_compile_options(-finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network OpenGL WebSockets)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gui
)

link_libraries(
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::OpenGL
    Qt6::WebSockets
)

set(CORE_SOURCES
    src/core/MemoryPool.hpp
    src/core/MemoryPool.cpp
    src/core/Tile.hpp
    src/core/TileManager.hpp
    src/core/TileManager.cpp
    src/core/BlendModes.hpp
    src/core/BlendModes.cpp
    src/core/Color.hpp
    src/core/Layer.hpp
    src/core/Layer.cpp
    src/core/VectorLayer.hpp
    src/core/VectorLayer.cpp
    src/core/AdjustmentLayer.hpp
    src/core/AdjustmentLayer.cpp
    src/core/TextLayer.hpp
    src/core/TextLayer.cpp
    src/core/GroupLayer.hpp
    src/core/GroupLayer.cpp
    src/core/Document.hpp
    src/core/Document.cpp
    src/core/UndoCommands.hpp
    src/core/UndoCommands.cpp
    src/core/StrokeSmoother.hpp
    src/core/DocumentSerializer.hpp
    src/core/DocumentSerializer.cpp
    src/core/AiService.hpp
    src/core/AiService.cpp
    src/core/LlmPromptService.hpp
    src/core/LlmPromptService.cpp
    src/core/ThreeDMath.hpp
    src/core/ThreeDCamera.hpp
    src/core/ThreeDCamera.cpp
    src/core/ThreeDModel.hpp
    src/core/ThreeDModel.cpp
    src/core/ThreeDRenderer.hpp
    src/core/ThreeDRenderer.cpp
    src/core/ThreeDGpuRenderer.hpp
    src/core/ThreeDGpuRenderer.cpp
    src/core/ThreeDLayer.hpp
    src/core/ThreeDLayer.cpp
    src/core/ThreeDPose.hpp
    src/core/ThreeDPose.cpp
    src/core/ThreeDMmdParser.hpp
    src/core/ThreeDMmdParser.cpp
    src/core/ThreeDVrmParser.hpp
    src/core/ThreeDVrmParser.cpp
    src/core/ThreeDAnimation.hpp
    src/core/ThreeDAnimation.cpp
    src/core/FilterProcessor.hpp
    src/core/FilterProcessor.cpp
    src/core/DisplayFilterManager.hpp
    src/core/DisplayFilterManager.cpp
)

set(GUI_SOURCES
    src/gui/MainWindow.hpp
    src/gui/MainWindow.cpp
    src/gui/PreviewWindow.hpp
    src/gui/PreviewWindow.cpp
    src/gui/ReferenceImageWindow.hpp
    src/gui/ReferenceImageWindow.cpp
    src/gui/FilterDialogs.hpp
    src/gui/FilterDialogs.cpp
    src/gui/CanvasDialogs.hpp
    src/gui/CanvasDialogs.cpp
    src/gui/CanvasWidget.hpp
    src/gui/CanvasWidget.cpp
    src/gui/CanvasContainer.hpp
    src/gui/CanvasContainer.cpp
    src/gui/ColorWheelWidget.hpp
    src/gui/ColorWheelWidget.cpp
    src/gui/ColorBoxWidget.hpp
    src/gui/ColorBoxWidget.cpp
    src/gui/ColorPaletteWidget.hpp
    src/gui/ColorPaletteWidget.cpp
    src/gui/ToolBoxWidget.hpp
    src/gui/ToolBoxWidget.cpp
    src/gui/SpinScaleWidget.hpp
    src/gui/SpinScaleWidget.cpp
    src/gui/BrushSettingsWidget.hpp
    src/gui/BrushSettingsWidget.cpp
    src/gui/LayerPanelWidget.hpp
    src/gui/LayerPanelWidget.cpp
    src/gui/NavigatorWidget.hpp
    src/gui/NavigatorWidget.cpp
    src/gui/TextBalloonDialog.hpp
    src/gui/TextBalloonDialog.cpp
    src/gui/TextSettingsWidget.hpp
    src/gui/TextSettingsWidget.cpp
    src/gui/RectSelectSettingsWidget.hpp
    src/gui/RectSelectSettingsWidget.cpp
    src/gui/EllipseSelectSettingsWidget.hpp
    src/gui/EllipseSelectSettingsWidget.cpp
    src/gui/LassoSelectSettingsWidget.hpp
    src/gui/LassoSelectSettingsWidget.cpp
    src/gui/ColorSelectSettingsWidget.hpp
    src/gui/ColorSelectSettingsWidget.cpp
    src/gui/BucketSettingsWidget.hpp
    src/gui/BucketSettingsWidget.cpp
    src/gui/PatternFillSettingsWidget.hpp
    src/gui/PatternFillSettingsWidget.cpp
    src/gui/AutoSelectSettingsWidget.hpp
    src/gui/AutoSelectSettingsWidget.cpp
    src/gui/BlurSettingsWidget.hpp
    src/gui/BlurSettingsWidget.cpp
    src/gui/SmudgeSettingsWidget.hpp
    src/gui/SmudgeSettingsWidget.cpp
    src/gui/GradientSettingsWidget.hpp
    src/gui/GradientSettingsWidget.cpp
    src/gui/WatercolorSettingsWidget.hpp
    src/gui/WatercolorSettingsWidget.cpp
    src/gui/CompositeBrushSettingsWidget.hpp
    src/gui/CompositeBrushSettingsWidget.cpp
    src/gui/BrushSettingsDialog.hpp
    src/gui/BrushSettingsDialog.cpp
    src/gui/VectorEraserSettingsWidget.hpp
    src/gui/VectorEraserSettingsWidget.cpp
    src/gui/VectorControlPointSettingsWidget.hpp
    src/gui/VectorControlPointSettingsWidget.cpp
    src/gui/VectorLineSettingsWidget.hpp
    src/gui/VectorLineSettingsWidget.cpp
    src/gui/VectorChangeLineSettingsWidget.hpp
    src/gui/VectorChangeLineSettingsWidget.cpp
    src/gui/VectorColorChangeSettingsWidget.hpp
    src/gui/VectorColorChangeSettingsWidget.cpp
    src/gui/VectorEllipseSettingsWidget.hpp
    src/gui/VectorEllipseSettingsWidget.cpp
    src/gui/VectorCircleSettingsWidget.hpp
    src/gui/VectorCircleSettingsWidget.cpp
    src/gui/VectorPolygonSettingsWidget.hpp
    src/gui/VectorPolygonSettingsWidget.cpp
    src/gui/VectorPressureSettingsWidget.hpp
    src/gui/VectorPressureSettingsWidget.cpp
    src/gui/NewCanvasDialog.hpp
    src/gui/NewCanvasDialog.cpp
    src/gui/PreferencesDialog.hpp
    src/gui/PreferencesDialog.cpp
    src/gui/ScatterSettingsWidget.hpp
    src/gui/ScatterSettingsWidget.cpp
    src/gui/ImageBrushSettingsWidget.hpp
    src/gui/ImageBrushSettingsWidget.cpp
    src/gui/TransformSettingsWidget.hpp
    src/gui/TransformSettingsWidget.cpp
    src/gui/ToneCurveWidget.hpp
    src/gui/ToneCurveWidget.cpp
    src/gui/LevelsWidget.hpp
    src/gui/LevelsWidget.cpp
    src/gui/AdjustmentSettingsWidget.hpp
    src/gui/AdjustmentSettingsWidget.cpp
    src/gui/PanelSettingsWidget.hpp
    src/gui/PanelSettingsWidget.cpp
    src/gui/AiAssistantDockWidget.hpp
    src/gui/AiAssistantDockWidget.cpp
    src/gui/AiComponentDownloader.hpp
    src/gui/AiComponentDownloader.cpp
    src/gui/LlmPromptDialog.hpp
    src/gui/LlmPromptDialog.cpp
    src/gui/ThreeDSettingsWidget.hpp
    src/gui/ThreeDSettingsWidget.cpp
)

add_executable(PaintSoftKABURAGI
    src/main.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)

target_link_libraries(PaintSoftKABURAGI PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

add_executable(test_undo
    tests/test_undo.cpp
    ${CORE_SOURCES}
)
target_link_libraries(test_undo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_layer_panel
    tests/test_layer_panel.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_layer_panel PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_auto_select
    tests/test_auto_select.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_auto_select PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_blur
    tests/test_blur.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_blur PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_smudge
    tests/test_smudge.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_smudge PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_gradient
    tests/test_gradient.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_gradient PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_brush_features
    tests/test_brush_features.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_brush_features PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_clipping
    tests/test_clipping.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_clipping PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_vector_layer
    tests/test_vector_layer.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_vector_layer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_document_io
    tests/test_document_io.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_document_io PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_clipboard
    tests/test_clipboard.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_clipboard PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_layer_menu
    tests/test_layer_menu.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_layer_menu PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_transform
    tests/test_transform.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_transform PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_adjustment_layer
    tests/test_adjustment_layer.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_adjustment_layer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

add_executable(test_text_tool
    tests/test_text_tool.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_text_tool PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

add_executable(test_panel_tool
    tests/test_panel_tool.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_panel_tool PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

add_executable(test_ai_service
    tests/test_ai_service.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_ai_service PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

add_executable(test_three_d
    tests/test_three_d.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_three_d PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

add_executable(test_ui_improvements
    tests/test_ui_improvements.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
)
target_link_libraries(test_ui_improvements PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

if (WIN32)
    # Windows GUIアプリケーションとしてビルド（起動時のコマンドプロンプトを非表示）
    set_target_properties(PaintSoftKABURAGI PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    get_target_property(_qt_core_loc Qt6::Core LOCATION)
    if (_qt_core_loc)
        get_filename_component(_qt_bin_dir "${_qt_core_loc}" DIRECTORY)
        add_custom_command(TARGET PaintSoftKABURAGI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_qt_bin_dir}/Qt6OpenGL.dll"
                "$<TARGET_FILE_DIR:PaintSoftKABURAGI>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_qt_bin_dir}/Qt6WebSockets.dll"
                "$<TARGET_FILE_DIR:PaintSoftKABURAGI>"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/translations"
                "$<TARGET_FILE_DIR:PaintSoftKABURAGI>/translations"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/pattern"
                "$<TARGET_FILE_DIR:PaintSoftKABURAGI>/pattern"
            COMMENT "Copying Qt6OpenGL.dll, Qt6WebSockets.dll, translations and pattern to output directory..."
        )
    endif()
endif()
