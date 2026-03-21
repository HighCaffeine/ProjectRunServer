@echo off
setlocal

:: Proto 파일 수정 시 자동으로 cpp, cs파일을 생성하는 배치파일입니다.
:: 수정했다면 실행하거나 TD 호출해주세요~

:: 1. 경로 설정
:: protoc.exe 실행 파일 위치
set PROTOC_PATH=..\thirdparty\protobuf\bin\protoc.exe

:: .proto 파일들이 있는 위치 (현재 폴더)
set PROTO_PATH=.

:: C++ 파일이 생성될 서버 폴더 경로
set CPP_OUT=..\Server

:: C# 파일이 생성될 유니티 폴더 경로
set CS_OUT=..\Client\Assets\Scripts\Network


:: 2. 출력 폴더 체크 및 생성

if not exist %CS_OUT% (
    echo [Notice] 유니티 네트워크 폴더 생성 중...
    mkdir %CS_OUT%
)

echo [Protobuf] 변환 작업을 시작합니다...


:: 3. 모든 .proto 파일을 빌드


for %%f in (%PROTO_PATH%\*.proto) do (
    echo 빌드 중: %%f
    %PROTOC_PATH% -I=%PROTO_PATH% --cpp_out=%CPP_OUT% --csharp_out=%CS_OUT% %%f
)

echo.
echo [Success] 모든 파일이 서버와 클라이언트로 복사되었습니다
pause