cmake_minimum_required(VERSION 3.10)

# 프로젝트 설정
project(CK_ProjectRun_Server LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 모든 경로 변수 설정
set(TP_DIR "${CMAKE_SOURCE_DIR}/thirdparty")
set(SENTRY_DIR "${TP_DIR}/sentry-native")
set(HIREDIS_DIR "${TP_DIR}/hiredis")
set(SPDLOG_DIR "${TP_DIR}/spdlog-1.x/include")

# 모든 소스 파일 (서버 + 내비게이션 소스코드)
# 서버 소스
file(GLOB_RECURSE SERVER_SOURCES "Server/*.cpp" "Server/*.h")

# Detour 소스
file(GLOB_RECURSE DETOUR_SOURCES 
    "${TP_DIR}/Detour/*.cpp" 
    "${TP_DIR}/Detour/*.h"
)

# DetourTileCache 소스
file(GLOB_RECURSE TILECACHE_SOURCES 
    "${TP_DIR}/recastnavigation/DetourTileCache/*.cpp" 
    "${TP_DIR}/recastnavigation/DetourTileCache/*.h"
)

# FastLZ 소스
file(GLOB_RECURSE FASTLZ_SOURCES 
    "${TP_DIR}/recastnavigation/RecastDemo/Contrib/fastlz/*.c" 
    "${TP_DIR}/recastnavigation/RecastDemo/Contrib/fastlz/*.h"
)

# 실행 파일 생성
add_executable(FinalProjectServer 
    ${SERVER_SOURCES} 
    ${DETOUR_SOURCES} 
    ${TILECACHE_SOURCES} 
    ${FASTLZ_SOURCES}
)

# 헤더 파일 경로 설정
target_include_directories(FinalProjectServer PRIVATE 
    "Server"
    "${SENTRY_DIR}"
    "${TP_DIR}/Detour/Include"
    "${TP_DIR}/recastnavigation/DetourTileCache/Include"
    "${TP_DIR}/recastnavigation/DetourTileCache/Source"
    "${TP_DIR}/recastnavigation/RecastDemo/Contrib/fastlz"
    "${HIREDIS_DIR}"
    "${SPDLOG_DIR}"
)

# 컴파일러 설정 및 전처리기 정의
if(MSVC)
    # 한글(UTF-8) 인코딩 강제 설정 및 경고 무시
    target_compile_options(FinalProjectServer PRIVATE "/utf-8")
    target_compile_definitions(FinalProjectServer PRIVATE 
        _WINSOCK_DEPRECATED_NO_WARNINGS
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# 라이브러리 연결
target_link_libraries(FinalProjectServer PRIVATE 
    "${SENTRY_DIR}/sentry.lib"
    "${HIREDIS_DIR}/VS-IDE/Release/hiredis.lib"
    "Winmm.lib" # 시간 및 멀티미디어 함수용
)

# 빌드 후 종속성 파일 자동 복사 (Windows 전용)
if(WIN32)
    add_custom_command(TARGET FinalProjectServer POST_BUILD
        # sentry.dll 복사
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SENTRY_DIR}/sentry.dll"
            "$<TARGET_FILE_DIR:FinalProjectServer>"
        
        # crashpad_handler.exe 복사
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SENTRY_DIR}/crashpad_handler.exe"
            "$<TARGET_FILE_DIR:FinalProjectServer>"
            
        COMMENT "Sentry binaries copying to output directory..."
    )
endif()