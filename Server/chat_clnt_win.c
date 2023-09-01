/*
    20장에서 사용한 채팅서버 클라이언트 예제.
    iocpchatserv_win과 함께 사용

*/

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define BUF_SIZE 100
#define NAME_SIZE 20

unsigned WINAPI SendMsg(void *arg);
unsigned WINAPI RecvMsg(void *arg);
void ErrorHandling(char *msg);

char name[NAME_SIZE] = "[DEFAULT]";
char msg[BUF_SIZE];

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    SOCKET hSock;
    SOCKADDR_IN servAdr;
    HANDLE hSndThread, hRcvThread;

    if (argc != 4) {
        printf("Usage: %s <IP> <port> <name> \n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    sprintf(name, argv[3]);
    hSock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&servAdr, 0, sizeof(servAdr));
    servAdr.sin_family = AF_INET;
    servAdr.sin_addr.s_addr = inet_addr(argv[1]);
    servAdr.sin_port = htons(atoi(argv[2]));

    if (connect(hSock, (SOCKADDR *)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
        ErrorHandling("connect() error");

    char nameSendMsg[NAME_SIZE + 1];
    sprintf(nameSendMsg, "1%s", name);
    send(hSock, nameSendMsg, NAME_SIZE + 1, 0);

    hSndThread = (HANDLE)_beginthreadex(NULL, 0, SendMsg, (void *)&hSock, 0, NULL);
    hRcvThread = (HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void *)&hSock, 0, NULL);

    WaitForSingleObject(hSndThread, INFINITE);
    WaitForSingleObject(hRcvThread, INFINITE);

    closesocket(hSock);
    WSACleanup();
    return 0;
}

unsigned WINAPI SendMsg(void *arg) // send thread main
{
    SOCKET hSock = *((SOCKET *)arg);
    char sendMsg[BUF_SIZE];

    while (1) {
        fgets(msg, BUF_SIZE, stdin);
        if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")) {
            closesocket(hSock); // 클라이언트 소켓 종료
            exit(EXIT_SUCCESS);
        }
        sprintf(sendMsg, "2%s", msg);
        send(hSock, sendMsg, strlen(sendMsg), 0);
    }
}

unsigned WINAPI RecvMsg(void *arg) // read thread main
{
    SOCKET hSock = *((SOCKET *)arg);
    char recvMsg[NAME_SIZE + BUF_SIZE + 1];
    int strLen;

    while (1) {
        strLen = recv(hSock, recvMsg, NAME_SIZE + BUF_SIZE - 1, 0);
        if (strLen == -1)
            return -1;
        recvMsg[strLen] = '\0';
        fputs(recvMsg + 1, stdout);
    }
}

void ErrorHandling(char *msg) {
    fputs(msg, stdout);
    fputc('\n', stdout);
    exit(EXIT_FAILURE);
}