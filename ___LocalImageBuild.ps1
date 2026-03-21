# 1. 로컬에서 직접 빌드 (깃허브 서버를 거치지 않음)
docker build -t highcaffeine/project-run-server:latest .

# 2. 도커 허브로 직접 업로드
docker push highcaffeine/project-run-server:latest

pause