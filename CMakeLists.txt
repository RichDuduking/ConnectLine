cmake_minimum_required(VERSION 3.16)

project(ResumePractice VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

set(RESUME_PRACTICE_ALGORITHM_SOURCES
    src/algorithm/polyline_algorithms.cpp
    src/algorithm/polyline_algorithms.h
)

set(RESUME_PRACTICE_DIAGNOSTICS_SOURCES
    src/diagnostics/connection_benchmark.cpp
    src/diagnostics/connection_benchmark.h
)

set(RESUME_PRACTICE_UI_SOURCES
    src/ui/practice_window.cpp
    src/ui/practice_window.h
    src/ui/my_graphics_view.h
)

qt_add_executable(ResumePractice
    src/main.cpp
    ${RESUME_PRACTICE_ALGORITHM_SOURCES}
    ${RESUME_PRACTICE_DIAGNOSTICS_SOURCES}
    ${RESUME_PRACTICE_UI_SOURCES}
)
target_include_directories(ResumePractice PRIVATE src)
target_link_libraries(ResumePractice PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
if (WIN32)
    target_link_libraries(ResumePractice PRIVATE psapi)
endif()

set_target_properties(ResumePractice PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS ResumePractice
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

include(CTest)
if (BUILD_TESTING)
    qt_add_executable(ResumePracticePolylineTest
        tests/polyline_algorithms_test.cpp
        ${RESUME_PRACTICE_ALGORITHM_SOURCES}
        ${RESUME_PRACTICE_DIAGNOSTICS_SOURCES}
    )
    target_include_directories(ResumePracticePolylineTest PRIVATE src)
    target_link_libraries(ResumePracticePolylineTest PRIVATE Qt${QT_VERSION_MAJOR}::Gui)
    if (WIN32)
        target_link_libraries(ResumePracticePolylineTest PRIVATE psapi)
    endif()
    add_test(NAME ResumePracticePolylineTest COMMAND ResumePracticePolylineTest)

    qt_add_executable(ResumePracticeLifecycleTest
        tests/connect_cancel_lifecycle_test.cpp
        ${RESUME_PRACTICE_ALGORITHM_SOURCES}
        ${RESUME_PRACTICE_DIAGNOSTICS_SOURCES}
        ${RESUME_PRACTICE_UI_SOURCES}
    )
    target_include_directories(ResumePracticeLifecycleTest PRIVATE src)
    target_link_libraries(ResumePracticeLifecycleTest PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
    if (WIN32)
        target_link_libraries(ResumePracticeLifecycleTest PRIVATE psapi)
    endif()

    add_test(NAME ResumePracticeLifecycleTest COMMAND ResumePracticeLifecycleTest)

    get_filename_component(QT_PLUGIN_DIR
        "${Qt6_DIR}/../../../plugins"
        ABSOLUTE
    )
    set_tests_properties(ResumePracticeLifecycleTest PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${QT_PLUGIN_DIR}"
    )
endif()

if (MSVC)
    add_compile_options(/fsanitize=address)
endif()
