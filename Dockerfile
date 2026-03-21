#FROM mcr.microsoft.com/windows/servercore:ltsc2022
FROM mcr.microsoft.com/windows/nanoserver:ltsc2022

WORKDIR /app

# 빌드 결과물, Sentry DLL, 그리고 맵 데이터 복사
COPY build/Release/FinalProjectServer.exe .
COPY build/Release/crashpad_handler.exe .
COPY Shared/MapData/all_tiles_tilecache.bin .

COPY thirdparty/dll/vcruntime140_1.dll .
COPY thirdparty/dll/vcruntime140.dll .
COPY thirdparty/dll/msvcp140.dll .

COPY thirdparty/sentry-native/sentry.dll .
COPY thirdparty/sentry-native/crashpad_handler.exe .

RUN mkdir .sentry-native

EXPOSE 5025/udp
EXPOSE 11021/tcp

ENTRYPOINT ["FinalProjectServer.exe"]