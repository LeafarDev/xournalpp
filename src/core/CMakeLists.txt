cmake_minimum_required(VERSION 3.12)
cmake_policy(VERSION 3.12)

file(GLOB_RECURSE xournalpp_sources *.cpp *.h)
file(GLOB_RECURSE xournalpp_ai_sources ${PROJECT_SOURCE_DIR}/src/ai/*.cpp ${PROJECT_SOURCE_DIR}/src/ai/*.h)
file(GLOB_RECURSE xournalpp_chat_sources ${PROJECT_SOURCE_DIR}/src/chat/*.cpp ${PROJECT_SOURCE_DIR}/src/chat/*.h)
file(GLOB_RECURSE xournalpp_latex_sources ${PROJECT_SOURCE_DIR}/src/latex/*.cpp ${PROJECT_SOURCE_DIR}/src/latex/*.h)
list(APPEND xournalpp_sources ${xournalpp_ai_sources} ${xournalpp_chat_sources} ${xournalpp_latex_sources})
if(APPLE)
    list(APPEND xournalpp_sources ${CMAKE_CURRENT_SOURCE_DIR}/gui/DockIconUpdater.mm)
endif()

if(NOT ENABLE_AUDIO)
    list(FILTER xournalpp_sources EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/src/core/audio/.*" )
    list(FILTER xournalpp_sources EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/src/core/control/AudioController.cpp" )
endif()
set(xournalpp_include_dirs "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")

add_library(xournalpp-core STATIC ${xournalpp_sources})
target_compile_features(xournalpp-core PUBLIC ${PROJECT_CXX_FEATURES})
target_compile_features(xournalpp-core PUBLIC cxx_std_20)
target_link_libraries(xournalpp-core
    PRIVATE xoj::util ${xournalpp_LDFLAGS} llama
        PUBLIC xoj::defaults xoj::external_modules
        )
if(APPLE)
    target_link_libraries(xournalpp-core PRIVATE "-framework PDFKit" "-framework AppKit")
endif()
target_include_directories(xournalpp-core PUBLIC ${xournalpp_include_dirs})
add_library(xoj::core ALIAS xournalpp-core)
