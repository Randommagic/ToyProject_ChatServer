#include <Winsock2.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define BUF_SIZE 100
#define NAME_SIZE 20

typedef struct {
    int messageType;
    char data[BUF_SIZE];
} MESSAGE_DATA;

unsigned WINAPI SendMsg(void *arg);
unsigned WINAPI RecvMsg(void *arg);

void SerializeMessage(MESSAGE_DATA *message, char *buffer);
void DeserializeMessage(char *buffer, MESSAGE_DATA *message);
void ErrorHandling(char *msg);

char name[NAME_SIZE] = "[DEFAULT]";

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    SOCKET hSock;
    SOCKADDR_IN servAdr;
    HANDLE hSndThread, hRcvThread;

    // if (argc != 4) {
    //     printf("Usage: %s <IP> <port> <name> \n", argv[0]);
    //     exit(EXIT_FAILURE);
    // }

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    sprintf(name, argv[1]);
    hSock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&servAdr, 0, sizeof(servAdr));
    servAdr.sin_family = AF_INET;
    // servAdr.sin_addr.s_addr = inet_addr(argv[1]);
    servAdr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // servAdr.sin_port = htons(atoi(argv[2]));
    servAdr.sin_port = htons(atoi("9999"));

    if (connect(hSock, (SOCKADDR *)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
        ErrorHandling("connect() error");

    // Îß? Ï≤òÏùå Î≥¥ÎÇº Î©îÏÑ∏Ïß? Ï§?Îπ? - ?ù¥Î¶? ?†ïÎ≥?
    MESSAGE_DATA initSendMessageData;
    memset(&initSendMessageData, 0, sizeof(MESSAGE_DATA));
    initSendMessageData.messageType = 1;
    memcpy(initSendMessageData.data, name, sizeof(name));
    initSendMessageData.data[sizeof(name)] = 0;

    char initSendMessageBuffer[sizeof(MESSAGE_DATA)];
    SerializeMessage(&initSendMessageData, initSendMessageBuffer);

    send(hSock, initSendMessageBuffer, sizeof(MESSAGE_DATA), 0);

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
    MESSAGE_DATA sendMessageData;

    char inputBuffer[BUF_SIZE];
    char sendMessageBuffer[sizeof(MESSAGE_DATA)];

    while (1) {
        fgets(inputBuffer, BUF_SIZE, stdin);

        if (!strcmp(inputBuffer, "q\n") || !strcmp(inputBuffer, "Q\n")) {
            shutdown(hSock, SD_SEND);
            return 0;
        }

        if (!strncmp(inputBuffer, "/r", 2)) {
            if (strlen(inputBuffer) - 3 >= NAME_SIZE) {
                printf("¿Ã∏ß¿Ã ≥ π´ ±‚∑ØøÎ\n");
                continue;
            }
            inputBuffer[strlen(inputBuffer) - 1] = '\0';
            memcpy(sendMessageData.data, inputBuffer + 3, NAME_SIZE);
            sendMessageData.messageType = 3;
        } else {
            sendMessageData.messageType = 2;
            memcpy(sendMessageData.data, inputBuffer, BUF_SIZE);
        }

        SerializeMessage(&sendMessageData, sendMessageBuffer);
        send(hSock, sendMessageBuffer, sizeof(MESSAGE_DATA), 0);
    }
}

unsigned WINAPI RecvMsg(void *arg) // read thread main
{
    SOCKET hSock = *((SOCKET *)arg);

    MESSAGE_DATA recvMessageData;
    char recvMessageBuffer[sizeof(MESSAGE_DATA)];

    int strLen;

    while (1) {
        strLen = recv(hSock, recvMessageBuffer, sizeof(MESSAGE_DATA), 0);
        if (strLen == SOCKET_ERROR)
            return -1;
        else if (strLen == 0) {
            shutdown(hSock, SD_RECEIVE);
            return 0;
        }

        DeserializeMessage(recvMessageBuffer, &recvMessageData);

        if (recvMessageData.messageType == 1) {
            printf("%s ¥‘¿Ã ¡¢º”«œºÃΩ¿¥œ¥Ÿ.\n", recvMessageData.data);
        } else if (recvMessageData.messageType == 2) {
            fputs(recvMessageData.data, stdout);
        } else if (recvMessageData.messageType == 3) { // ¿Ã∏ß ∫Ø∞Ê
            printf("%s ¿∏∑Œ ¿Ã∏ß¿ª ∫Ø∞Ê«œºÃΩ¿¥œ¥Ÿ.\n", recvMessageData.data);
        } else if (recvMessageData.messageType == 9) {
            printf("%s ¥‘¿Ã ≈¿Â«œºÃΩ¿¥œ¥Ÿ.\n", recvMessageData.data);
        }
    }
}

void SerializeMessage(MESSAGE_DATA *message, char *buffer) { memcpy(buffer, message, sizeof(MESSAGE_DATA)); }

void DeserializeMessage(char *buffer, MESSAGE_DATA *message) { memcpy(message, buffer, sizeof(MESSAGE_DATA)); }

void ErrorHandling(char *msg) {
    fputs(msg, stdout);
    fputc('\n', stdout);
    exit(EXIT_FAILURE);
}