# ExtraAssignment
IoT 실습

## Requirements
- More than 2 Servers

## Quick start
1. Open download this repository with 'git clone https://github.com/hwangbo98/ExtraAssignment.git'
2. Run 'make' to create executable file.
3. Run program on server side.
   ```
   ./fileserver 203.252.112.25 3131 test.jpg
   ```
4. Run program on client side. -> Client가 3명이 열려야 하기 때문에 26번 서버에서 3번을 같이 실행시켜줘야 함.
   ```
   ./fileclient 203.252.112.25 3131 .
   ```
5. Transfer peer info and File from server to Client.
   
