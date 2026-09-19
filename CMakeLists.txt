cmake_minimum_required(VERSION 3.20)

project(AmuletsArmor LANGUAGES C CXX)

option(AA_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(AA_ENABLE_ASAN "Enable AddressSanitizer for debugging" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(AA_EXE_NAME "amulets-armor")

file(GLOB AA_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.C")

list(REMOVE_ITEM AA_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/DEBUGBOR.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/TOSDATA.C"
)

set(AA_PLATFORM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Build/Windows/VC2013/AA/main.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Build/Windows/VC2013/AA/direct.cpp"
)

if(NOT EMSCRIPTEN)
    # IPX-over-UDP networking; browsers have no raw UDP.
    list(APPEND AA_PLATFORM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Source/Win32/ipx_client.cpp")
endif()

add_executable(${AA_EXE_NAME}
    ${AA_CORE_SOURCES}
    ${AA_PLATFORM_SOURCES}
)

add_executable(aa-character-dump
    "${CMAKE_CURRENT_SOURCE_DIR}/Utils/character_dump_json.c"
)

# The project uses uppercase .C files as C sources; force C language mode.
set_source_files_properties(${AA_CORE_SOURCES} PROPERTIES LANGUAGE C)

if(NOT EMSCRIPTEN)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL REQUIRED IMPORTED_TARGET sdl)
    pkg_check_modules(SDL_NET REQUIRED IMPORTED_TARGET SDL2_net)
endif()

target_include_directories(${AA_EXE_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        "${CMAKE_CURRENT_SOURCE_DIR}/Build/Windows/VC2013/AA"
)

target_include_directories(aa-character-dump
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

if(NOT EMSCRIPTEN)
    target_link_libraries(${AA_EXE_NAME}
        PRIVATE
            PkgConfig::SDL
            PkgConfig::SDL_NET
    )
endif()

target_compile_definitions(${AA_EXE_NAME}
    PRIVATE
        WIN32=1
        NO_ASSEMBLY=1
        _CRT_SECURE_NO_WARNINGS=1
        _MBCS=1
        cdecl=
        _cdecl=
        __cdecl=
        __fastcall=
)

if(EMSCRIPTEN)
    target_compile_definitions(${AA_EXE_NAME} PRIVATE WIN_IPX=0)
else()
    target_compile_definitions(${AA_EXE_NAME} PRIVATE WIN_IPX=1)
endif()

if(UNIX)
    target_compile_definitions(${AA_EXE_NAME} PRIVATE TARGET_UNIX=1)
    target_compile_definitions(aa-character-dump PRIVATE TARGET_UNIX=1)

    # Stand-ins for Windows/DOS headers (io.h, direct.h, conio.h, ...). Kept out
    # of Include/ so they cannot shadow the real headers in the Visual Studio build.
    target_include_directories(${AA_EXE_NAME} BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Include/unix")
    target_include_directories(aa-character-dump BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Include/unix")
endif()

if(UNIX AND NOT APPLE)
    # Case-insensitive open()/fopen() (see Source/Unix/casefile.c). Fortify
    # rewrites open() to __open_2, which would bypass the wrapper.
    target_sources(${AA_EXE_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source/Unix/casefile.c")
    target_compile_options(${AA_EXE_NAME} PRIVATE -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0)
    target_link_options(${AA_EXE_NAME} PRIVATE -Wl,--wrap=open -Wl,--wrap=fopen)
endif()

if(APPLE)
    # sdl12-compat loads SDL2 dynamically; add Homebrew and pkg-config library
    # directories to the runtime search path so startup does not depend on shell env.
    set(AA_RPATH_DIRS
        "/usr/local/opt/sdl12-compat/lib"
        "/usr/local/opt/sdl2/lib"
    )

    if(SDL_LIBRARY_DIRS)
        list(APPEND AA_RPATH_DIRS ${SDL_LIBRARY_DIRS})
    endif()

    if(SDL_NET_LIBRARY_DIRS)
        list(APPEND AA_RPATH_DIRS ${SDL_NET_LIBRARY_DIRS})
    endif()

    list(REMOVE_DUPLICATES AA_RPATH_DIRS)

    set_target_properties(${AA_EXE_NAME} PROPERTIES
        BUILD_RPATH "${AA_RPATH_DIRS}"
        INSTALL_RPATH "${AA_RPATH_DIRS}"
    )
endif()

if(AA_WARNINGS_AS_ERRORS)
    target_compile_options(${AA_EXE_NAME} PRIVATE -Werror)
endif()

if(AA_ENABLE_ASAN)
    target_compile_options(${AA_EXE_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(${AA_EXE_NAME} PRIVATE -fsanitize=address)
endif()

if(EMSCRIPTEN)
    set(AA_WEB_INITIAL_MEMORY "64MB" CACHE STRING "Initial wasm heap (raise for AddressSanitizer)")
    # The game blocks in its own loops (Sleep + frame update), so Asyncify
    # turns those into yield points back to the browser.
    # Old K&R-style C: recent clang turns these into hard errors.
    target_compile_options(${AA_EXE_NAME} PRIVATE
        -sUSE_SDL=1
        -Wno-error=implicit-function-declaration
        -Wno-error=int-conversion
        -Wno-error=incompatible-pointer-types
        -Wno-error=incompatible-function-pointer-types
        -Wno-implicit-int
    )

    # Parse SDL's headers first with WIN32 hidden (see the file). emcc puts
    # its own SDL include dir ahead of -I, so a shim SDL.h would not win.
    target_compile_options(${AA_EXE_NAME} PRIVATE
        "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/Build/Emscripten/aa_sdl_shim.h")

    set_target_properties(${AA_EXE_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${AA_EXE_NAME} PRIVATE
        -sUSE_SDL=1
        -sASYNCIFY
        -sASYNCIFY_STACK_SIZE=131072
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=${AA_WEB_INITIAL_MEMORY}
        -sSTACK_SIZE=1MB
        -sFORCE_FILESYSTEM=1
        -sEXPORTED_RUNTIME_METHODS=FS,wasmTable
        -lidbfs.js
        "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/Exe@/game"
        "SHELL:--exclude-file *.exe" "SHELL:--exclude-file *.EXE"
        "SHELL:--exclude-file *.bat" "SHELL:--exclude-file *.386"
        "SHELL:--exclude-file *.dll"
    )
else()
    set(AA_RUNTIME_DIR "${CMAKE_BINARY_DIR}/runtime")

    add_custom_command(TARGET ${AA_EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${AA_RUNTIME_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Exe"
            "${AA_RUNTIME_DIR}"
        COMMENT "Copying runtime assets to ${AA_RUNTIME_DIR}"
    )
endif()
