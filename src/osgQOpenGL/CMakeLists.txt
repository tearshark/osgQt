IF ( Qt5Widgets_FOUND )
    include(GenerateExportHeader)

    SET(LIB_NAME osgQOpenGL)
    SET(HEADER_INSTALL_DIR osgQOpenGL)

    set(CMAKE_INCLUDE_CURRENT_DIR yes)

    SET(HEADER_PATH ../../include/osgQOpenGL)

    SET(SOURCE_H 
        ${HEADER_PATH}/osgQOpenGLWidget
        ${HEADER_PATH}/osgQOpenGLWindow
        ${HEADER_PATH}/OSGRenderer
    )

    qt5_wrap_cpp(SOURCES_H_MOC ${SOURCE_H} #[[OPTIONS ${MOC_OPTIONS}]])

    SET(TARGET_H
        ${SOURCE_H}
        ${HEADER_PATH}/GraphicsWindowEx
        ${HEADER_PATH}/CullVisitorEx
        ${HEADER_PATH}/RenderStageEx
        ${HEADER_PATH}/StateEx
        ${CMAKE_CURRENT_BINARY_DIR}/Export
    )


    SET(TARGET_SRC
        GraphicsWindowEx.cpp
        osgQOpenGLWidget.cpp
        osgQOpenGLWindow.cpp
        OSGRenderer.cpp
        CullVisitorEx.cpp
        RenderStageEx.cpp
        ${SOURCES_H_MOC}
        ${OPENSCENEGRAPH_VERSIONINFO_RC}
    )

    SET(TARGET_LIBRARIES Qt5::Widgets Qt5::OpenGL)
    SET(TARGET_LIBRARIES_VARS
        OSG_LIBRARY
        OSGMATH_LIBRARY
        OSGUTIL_LIBRARY
        OSGVIEWER_LIBRARY
        OSGGA_LIBRARY
        OSGDB_LIBRARY
        OPENTHREADS_LIBRARY
    )

    SETUP_LIBRARY(${LIB_NAME})
    set_target_properties(${LIB_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/")

    generate_export_header(${LIB_NAME} EXPORT_FILE_NAME "Export")

ENDIF()
