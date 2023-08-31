/*
    IOCPChatServ_win.c
    IOCP 를 사용한 채팅서버

    클라이언트 접속 해제 부분에서, GetQueuedCompletionStatus error 생김.
*/

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winerror.h>
#include <winsock2.h>

#define MAX_CLNT 256
#define BUF_SIZE 100
#define READ 3
#define WRITE 5

typedef struct {
    SOCKET hClntSock;
    SOCKADDR_IN clntAdr;
} PER_HANDLE_DATA, *LPPER_HANDLE_DATA;

typedef struct {
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[BUF_SIZE];
    int rwMode;
    int refCnt;
} PER_IO_DATA, *LPPER_IO_DATA;

DWORD WINAPI EchoThreadMain(LPVOID);

void ClientDisconnected(SOCKET sock, LPPER_HANDLE_DATA handleInfo, LPPER_IO_DATA ioInfo);
void ErrorHandling(char *);

LPPER_HANDLE_DATA clntHandles[MAX_CLNT];

int clntCnt = 0;
HANDLE hMutex;

int main(int argc, char *argv[]) {
    if (argc != 2)
        exit(1);

    WSADATA wsaData;
    HANDLE hComPort;
    SYSTEM_INFO sysInfo;
    LPPER_IO_DATA ioInfo;
    LPPER_HANDLE_DATA handleInfo;

    SOCKET hServSock;
    SOCKADDR_IN servAdr;
    DWORD recvBytes, flags = 0;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup Error!");

    // IOCP를 위한 Completion Port 생성.
    if ((hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) == NULL) {
        printf("Errorcode : %d \n", WSAGetLastError());
        ErrorHandling("CreateIocompletionPort connecting to socket error");
    }

    // 코어의 수 만큼 IO를 담당할 쓰레드 생성
    GetSystemInfo(&sysInfo);
    for (int i = 0; i < sysInfo.dwNumberOfProcessors; i++)
        _beginthreadex(NULL, 0, EchoThreadMain, (LPVOID)hComPort, 0, NULL);

    // WSASocket 설정
    if ((hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
        ErrorHandling("socket creation error");
    memset(&servAdr, 0, sizeof(servAdr));
    servAdr.sin_family = AF_INET;
    servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAdr.sin_port = htons(atoi(argv[1]));

    if (bind(hServSock, (SOCKADDR *)&servAdr, sizeof(servAdr)) == SOCKET_ERROR) {
        printf("code : %d", WSAGetLastError());
        ErrorHandling("bind error");
    }

    if (listen(hServSock, 5) == SOCKET_ERROR)
        ErrorHandling("listen error");
    // 소켓 설정 끝

    // Mutex init
    hMutex = CreateMutex(NULL, FALSE, NULL);

    while (1) {
        SOCKET hClntSock;
        SOCKADDR_IN clntAdr;
        int addrLen = sizeof(clntAdr);
        if ((hClntSock = accept(hServSock, (SOCKADDR *)&clntAdr, &addrLen)) == SOCKET_ERROR)
            ErrorHandling("accept error");

        // 클라이언트의 SOCKET 핸들과 adr을 저장할 handleinfo 구조체 동적 할당
        handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
        // 책에는 이 memset이 빠져 있다..
        memset(handleInfo, 0, sizeof(PER_HANDLE_DATA));
        handleInfo->hClntSock = hClntSock;
        memcpy(&(handleInfo->clntAdr), &clntAdr, addrLen);

        // 클라이언트의 소켓과 앞에서 생성한 Completion Port 연결.
        CreateIoCompletionPort((HANDLE)hClntSock, hComPort, (ULONG_PTR)handleInfo, 0);

        // WSARecv 함수 호출에 필요한 Overlapper와 WSABUF 구조체를 담고있는 ioInfo 구조체 동적 할당
        ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
        memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
        ioInfo->wsaBuf.len = BUF_SIZE;
        ioInfo->wsaBuf.buf = ioInfo->buffer;
        ioInfo->rwMode = READ;
        ioInfo->refCnt = 0;

        // 클라이언트 목록에 새로 접속한 클라이언트 추가
        WaitForSingleObject(&hMutex, INFINITE);
        clntHandles[clntCnt++] = handleInfo;
        ReleaseMutex(&hMutex);

        printf("Connected Client IP : %s \n", inet_ntoa(handleInfo->clntAdr.sin_addr));

        WSARecv(handleInfo->hClntSock, &(ioInfo->wsaBuf), 1, &recvBytes, &flags, &(ioInfo->overlapped), NULL);
    }
}

// Completion Port Object에 할당된 쓰레드
DWORD WINAPI EchoThreadMain(LPVOID pComport) {

    HANDLE hComport = (HANDLE)pComport;
    SOCKET sock;
    DWORD bytesTrans;
    LPPER_HANDLE_DATA handleInfo;
    LPPER_IO_DATA ioInfo;
    DWORD flags = 0;

    while (1) {
        // IO가 완료되고, 이에 대한 정보다 등록되었을때 반환.
        BOOL CPstatus = GetQueuedCompletionStatus(hComport, &bytesTrans, (LPDWORD)&handleInfo, (LPOVERLAPPED *)&ioInfo, INFINITE);
        if (CPstatus == FALSE) {
            if (WSAGetLastError() == ERROR_NETNAME_DELETED) { // Client Hard Close
                ClientDisconnected(sock, handleInfo, ioInfo);
                continue;
            } else
                ErrorHandling("GetQueuedCompletionStatus error");
        }

        sock = handleInfo->hClntSock;

        if (ioInfo->rwMode == READ) {
            puts("message received!");

            // EOF - Client Disconnect - Client Soft Close
            if (bytesTrans == 0) {
                // 클라이언트 리스트에서 접속종료한 클라이언트 삭제
                ClientDisconnected(sock, handleInfo, ioInfo);
                continue;
            }
            // 연결한 모든 클라이언트에게 데이터 전송.
            memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
            ioInfo->wsaBuf.len = bytesTrans;
            ioInfo->rwMode = WRITE;

            WaitForSingleObject(&hMutex, INFINITE);
            for (int i = 0; i < clntCnt; i++) {
                ++(ioInfo->refCnt);
                WSASend(clntHandles[i]->hClntSock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped), NULL);
            }
            ReleaseMutex(&hMutex);

            // 한명의 클라이언트에게 데이터 전송
            /*/
            memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
            ioInfo->wsaBuf.len = bytesTrans;
            ioInfo->rwMode = WRITE;
            // WSASend로 보낸 ioInfo는 free됨
            WSASend(sock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped), NULL);
            */

            // 클라이언트한테서 받을 buf와 wsabuf 등 io_data 공간을 미리 동적 할당 해놓음.
            ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
            memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
            ioInfo->wsaBuf.len = BUF_SIZE;
            ioInfo->wsaBuf.buf = ioInfo->buffer;
            ioInfo->rwMode = READ;
            WSARecv(sock, &(ioInfo->wsaBuf), 1, NULL, &flags, &(ioInfo->overlapped), NULL);

        } else {
            WaitForSingleObject(&hMutex, INFINITE);
            --(ioInfo->refCnt);
            ReleaseMutex(&hMutex);

            if (ioInfo->refCnt <= 0) {
                printf("Message sent to %d clients! \n", clntCnt);
                free(ioInfo);
            }
        }
    }
    return 0;
}

void ClientDisconnected(SOCKET sock, LPPER_HANDLE_DATA handleInfo, LPPER_IO_DATA ioInfo) {

    WaitForSingleObject(&hMutex, INFINITE);
    for (int i = 0; i < clntCnt; i++) {
        if (clntHandles[i]->hClntSock == sock) {
            printf("Client Disconnected from %s \n", inet_ntoa(clntHandles[i]->clntAdr.sin_addr));
            while (i++ < clntCnt - 1)
                clntHandles[i] = clntHandles[i + 1];
            break;
        }
    }
    clntCnt--;
    ReleaseMutex(&hMutex);
    closesocket(sock);
    free(handleInfo);
    free(ioInfo);
}

void ErrorHandling(char *msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}