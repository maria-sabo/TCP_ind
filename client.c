#include <stdio.h>
#include <string.h>
#include <windows.h>

#define MESSAGE_SIZE 10000

int readN(int socket, char *buf);

int main(int argc, char **argv) {

    WSADATA lpWSAData;
    if (WSAStartup(MAKEWORD(1, 1), &lpWSAData) != 0) return 0;

    struct sockaddr_in peer;
    peer.sin_family = AF_INET;

    char inputBuf[MESSAGE_SIZE];

    int s;

    for (;;) {
        printf("Enter IP address: \n");
        fflush(stdout);
        memset(inputBuf, 0, sizeof(inputBuf));
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';
        peer.sin_addr.s_addr = inet_addr(inputBuf);

        printf("Enter server port number: \n");
        fflush(stdout);
        memset(inputBuf, 0, sizeof(inputBuf));
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';
        peer.sin_port = htons(atoi(inputBuf));

        s = socket(AF_INET, SOCK_STREAM, 0); // создание сокета
        if (s == INVALID_SOCKET) {
            printf("Cannot create socket...\n");
            fflush(stdout);
            continue;
        }
        // подключение к серверу
        int rc = connect(s, (struct sockaddr *) &peer, sizeof(peer));
        if (rc == SOCKET_ERROR) {
            printf("Cannot connect to server \n");
            fflush(stdout);
            continue;
        } else {
            printf("Connected to server successfully! \n");
            fflush(stdout);
            break;
        }
    }

    char message[MESSAGE_SIZE] = {0};
    char login[MESSAGE_SIZE] = {0};
    for (;;) {
        for (;;) {
            printf("\nEnter your login: \n");
            fflush(stdout);
            fgets(login, MESSAGE_SIZE, stdin);
            login[strlen(login) - 1] = '\0';
            send(s, login, sizeof(login), 0);

            printf("Enter your password: \n");
            fflush(stdout);
            memset(inputBuf, 0, sizeof(inputBuf));
            fgets(inputBuf, MESSAGE_SIZE, stdin);
            inputBuf[strlen(inputBuf) - 1] = '\0';
            send(s, inputBuf, sizeof(inputBuf), 0);

            memset(inputBuf, 0, sizeof(inputBuf));
            readN(s, inputBuf);
            if ('-' == inputBuf[0]) {
                printf("Incorrect password. Try again \n");
                fflush(stdout);
                continue;
            } else {
                printf("Successful \n");
                fflush(stdout);
                break;
            }
        }
        printf("Input messages: \n");
        fflush(stdout);
        int exit = 0;
        for (;;) {
            fgets(inputBuf, sizeof(inputBuf), stdin);
            inputBuf[strlen(inputBuf) - 1] = '\0';

            if (!strcmp("-quit", inputBuf)) {
                exit = 1;
                break;
            } else {
                // передача данных
                send(s, inputBuf, sizeof(inputBuf), 0);
                if (readN(s, message) == -1) {
                    printf("Cannot read messages from server \n");
                    fflush(stdout);
                    exit = 1;
                    break;
                } else {
                    printf("Message from server: %s\n", message);
                    if (!strcmp("-logout", message)) {
                        exit = 0;
                        break;
                    }
                    if (!strcmp("x", message)){
                        exit = 1;
                        break;
                    }
                    //if (!strcmp("-who", message)) {}
                    fflush(stdout);
                }
                memset(inputBuf, 0, MESSAGE_SIZE);
            }
        }
        if (exit) break;
    }
    shutdown(s, 2);
    closesocket(s);
    printf("Client terminated \n");
    fflush(stdout);
    return 0;
}


int readN(int socket, char *buf) {
    int result = 0;
    int readedBytes = 0;
    int messageSize = MESSAGE_SIZE;
    while (messageSize > 0) {
        readedBytes = recv(socket, buf + result, messageSize, 0);
        if (readedBytes <= 0) {
            return -1;
        }
        result += readedBytes;
        messageSize -= readedBytes;
    }
    return result;
}
