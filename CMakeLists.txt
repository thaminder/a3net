cmake_minimum_required(VERSION 3.16)
project(A3Net VERSION 1.0 LANGUAGES CXX)

# A3Net - Ersatz fuer DirectPlay in Anstoss 3
#
# Windows (Ziel): baut dinput.dll (32 Bit!), die in den Spielordner kopiert wird.
# Linux/macOS:    baut nur das Testprogramm a3net_test (Netzwerkkern + DirectPlay-Schicht).

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(A3_CORE
    src/log.cpp
    src/netio.cpp
    src/session.cpp
    src/directplay.cpp
)

if(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR
            "Anstoss 3 ist ein 32-Bit-Programm - die DLL muss ebenfalls 32 Bit sein.\n"
            "Visual Studio: Preset 'msvc-x86' bzw. Kit 'amd64_x86'/'x86' waehlen.\n"
            "MinGW: den i686-Compiler (MSYS2 'mingw32') verwenden.")
    endif()

    include(CheckIncludeFileCXX)
    check_include_file_cxx(natupnp.h A3_HAVE_NATUPNP)

    add_library(a3net SHARED ${A3_CORE}
        src/win/platform_win.cpp
        src/win/dinput_proxy.cpp
        src/win/dinput.def
    )
    set_target_properties(a3net PROPERTIES OUTPUT_NAME "dinput" PREFIX "")
    target_compile_definitions(a3net PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601
                               _CRT_SECURE_NO_WARNINGS)
    if(A3_HAVE_NATUPNP)
        target_compile_definitions(a3net PRIVATE A3_HAVE_NATUPNP)
    endif()
    target_link_libraries(a3net PRIVATE ws2_32 ole32 oleaut32 user32)

    if(MSVC)
        # Statische Laufzeit: keine VC++-Redistributable auf den Spiel-PCs noetig
        set_property(TARGET a3net PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(a3net PRIVATE /W3 /utf-8)
    else()
        target_link_options(a3net PRIVATE -static -static-libgcc -static-libstdc++
                            -Wl,--enable-stdcall-fixup -Wl,--kill-at)
    endif()

    # Testprogramm auch unter Windows
    add_executable(a3net_test tests/test_session.cpp ${A3_CORE})
    target_compile_definitions(a3net_test PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601 _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(a3net_test PRIVATE ws2_32)
    if(MSVC)
        set_property(TARGET a3net_test PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        target_link_options(a3net_test PRIVATE -static)
    endif()

    # a3net.ini neben die gebaute DLL legen
    add_custom_command(TARGET a3net POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/a3net.ini" "$<TARGET_FILE_DIR:a3net>/a3net.ini")

    # Optional: nach dem Bauen direkt in den Spielordner kopieren
    set(A3_GAME_DIR "" CACHE PATH "Spielordner von Anstoss 3 (optional, fuer automatisches Kopieren)")
    if(A3_GAME_DIR)
        add_custom_command(TARGET a3net POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:a3net> "${A3_GAME_DIR}/dinput.dll"
            COMMENT "Kopiere dinput.dll nach ${A3_GAME_DIR}")
    endif()
else()
    find_package(Threads REQUIRED)
    add_executable(a3net_test tests/test_session.cpp ${A3_CORE})
    target_link_libraries(a3net_test PRIVATE Threads::Threads)
    target_compile_options(a3net_test PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
