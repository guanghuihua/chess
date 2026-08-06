cmake_minimum_required(VERSION 3.16)

project(chess VERSION 0.1 LANGUAGES CXX)

enable_testing()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql Network)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        xiangqi_board_widget.cpp
        xiangqi_board_widget.h
        xiangqi_game.cpp
        xiangqi_game.h
        game_database.cpp
        game_database.h
        pikafish_analyzer.cpp
        pikafish_analyzer.h
        deepseek_coach.cpp
        deepseek_coach.h
        credential_store.cpp
        credential_store.h
        json_object_extractor.cpp
        json_object_extractor.h
        training_dialog.cpp
        training_dialog.h
        profile_dashboard_widget.cpp
        profile_dashboard_widget.h
        game_review_dialog.cpp
        game_review_dialog.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(chess
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET chess APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(chess SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(chess
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(chess PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
)
if(WIN32)
    target_link_libraries(chess PRIVATE advapi32)
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.chess)
endif()
set_target_properties(chess PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS chess
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(chess)
endif()

add_executable(training_smoke_test
    tests/training_smoke_test.cpp
    xiangqi_game.cpp
    xiangqi_game.h
    game_database.cpp
    game_database.h
    pikafish_analyzer.cpp
    pikafish_analyzer.h
)
target_link_libraries(training_smoke_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
)
target_include_directories(training_smoke_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME training_smoke_test COMMAND training_smoke_test)

add_executable(xiangqi_undo_test
    tests/xiangqi_undo_test.cpp
    xiangqi_game.cpp
    xiangqi_game.h
)
target_include_directories(xiangqi_undo_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME xiangqi_undo_test COMMAND xiangqi_undo_test)

add_executable(training_ui_smoke_test
    tests/training_ui_smoke_test.cpp
    training_dialog.cpp
    training_dialog.h
    xiangqi_board_widget.cpp
    xiangqi_board_widget.h
    xiangqi_game.cpp
    xiangqi_game.h
    game_database.cpp
    game_database.h
    game_review_dialog.cpp
    game_review_dialog.h
    pikafish_analyzer.cpp
    pikafish_analyzer.h
)
target_link_libraries(training_ui_smoke_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
)
target_include_directories(training_ui_smoke_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME training_ui_smoke_test COMMAND training_ui_smoke_test)

add_executable(multi_user_profile_test
    tests/multi_user_profile_test.cpp
    game_database.cpp
    game_database.h
    xiangqi_game.cpp
    xiangqi_game.h
)
target_link_libraries(multi_user_profile_test PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
)
target_include_directories(multi_user_profile_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME multi_user_profile_test COMMAND multi_user_profile_test)

add_executable(json_response_test
    tests/json_response_test.cpp
    json_object_extractor.cpp
    json_object_extractor.h
)
target_link_libraries(json_response_test PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_include_directories(json_response_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME json_response_test COMMAND json_response_test)
