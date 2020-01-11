#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <ctype.h>
#include <io.h>
#include <time.h>

#define MESSAGE_SIZE 10000

struct CLIENT {
    int number;
    HANDLE thread;
    int socket;
    char *address;
    int port;
    char login[MESSAGE_SIZE];
} *clients;

struct Client_Login {
    char login[MESSAGE_SIZE];
    char password[MESSAGE_SIZE];
    //char commands[MESSAGE_SIZE];
} *clients_login;
int num_login = 0;

DWORD WINAPI listeningThread(void *info);

DWORD WINAPI clientHandler(void *clientIndex);

void readMessages();

int readN(int socket, char *buf);

void *kickClient(int number);

int numberOfClients = 0;
HANDLE mutex;

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Wrong command line format (server.exe ip port) \n");
        exit(1);
    }
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(atoi(argv[2]));
    local.sin_addr.s_addr = inet_addr(argv[1]);

    WSADATA lpWSAData;
    // поддерживается ли запрошенная версия DLL
    if (WSAStartup(MAKEWORD(1, 1), &lpWSAData) != 0) return 0;

    // создание сокета
    int listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == INVALID_SOCKET) {
        perror("Cannot create socket to listen \n");
        exit(1);
    }
    // связывание сокета с портом
    int rc = bind(listeningSocket, (struct sockaddr *) &local, sizeof(local));
    if (rc == SOCKET_ERROR) {
        perror("Cannot bind socket \n");
        exit(1);
    }

    // перевод сокета в слушающий режим
    // 5 - максимальное число запросов на соединение, которые будут помещены в очередь
    if (listen(listeningSocket, 5) == SOCKET_ERROR) {
        perror("Cannot listen socket \n");
        exit(1);
    }

    // создание мьютекса
    mutex = CreateMutex(NULL, FALSE, NULL);
    if (mutex == NULL) {
        perror("Failed to create mutex \n");
        exit(1);
    }

    // создание  потока
    HANDLE thread = CreateThread(NULL, 0, &listeningThread, &listeningSocket, 0, NULL);
    if (thread == NULL) {
        printf("Cannot create listening thread \n");
        fflush(stdout);
        exit(1);
    }

    printf("Use -list to show all online users \n");
    printf("Use -kick No to kick client by number \n");
    printf("Use -quit to shutdown server \n");
    printf("Waiting for input messages...\n");
    fflush(stdout);

    readMessages();

    shutdown(listeningSocket, 2);
    closesocket(listeningSocket);
    WaitForSingleObject(thread, INFINITE);
    printf("Server terminated \n");
    WSACleanup();
    return 0;
}

void readMessages() {
    char buf[100];

    // основной цикл работы
    for (;;) {
        memset(buf, 0, 100);
        fgets(buf, 100, stdin);
        buf[strlen(buf) - 1] = '\0';

        if (!strcmp("-list", buf)) {
            printf("Clients online: \n");
            WaitForSingleObject(&mutex, INFINITE);
            for (int i = 0; i < numberOfClients; i++) {
                if (clients[i].socket != INVALID_SOCKET)
                    printf(" %d          %s          %d          %s\n", clients[i].number, clients[i].address,
                           clients[i].port, clients[i].login);
            }
            ReleaseMutex(&mutex);
            fflush(stdout);
        } else if (!strcmp("-quit", buf)) {
            break;
        } else {
            char *string = strtok(buf, " ");
            if (!strcmp("-kick", string)) {
                string = strtok(NULL, " ");
                if (string == NULL)
                    printf("Incorrect command. Enter'-kick No' \n");
                else {
                    int num = atoi(string);
                    if (string[0] != '0' && num == 0) {
                        printf("Incorrect command. Enter'-kick No' \n");
                    } else {
                        kickClient(num);
                    }
                }
            } else {
                printf("Undefined message \n");
                fflush(stdout);
                continue;
            }
        }
    }
}

DWORD WINAPI listeningThread(void *info) {
    int listener = *((int *) info);

    int new, clientIndex;
    struct sockaddr_in addr;
    int length = sizeof(addr);

    for (;;) {
        // прием соединений
        new = accept(listener, (struct sockaddr *) &addr, &length);
        if (new == INVALID_SOCKET) {
            break;
        }

        WaitForSingleObject(&mutex, INFINITE);
        clients = (struct CLIENT *) realloc(clients, sizeof(struct CLIENT) * (numberOfClients + 1));
        clients[numberOfClients].socket = new;
        clients[numberOfClients].address = inet_ntoa(addr.sin_addr);
        clients[numberOfClients].port = addr.sin_port;
        clients[numberOfClients].number = numberOfClients;
        clientIndex = numberOfClients;
        clients[numberOfClients].thread = CreateThread(NULL, 0, &clientHandler, &clientIndex, 0, NULL);
        if (clients[numberOfClients].thread == NULL) {
            printf("Cannot create thread for new client \n");
            fflush(stdout);
            continue;
        } else {
            printf("New client connected! \n");
            fflush(stdout);
        }
        ReleaseMutex(&mutex);
        numberOfClients++;
    }
    //printf("Server termination process started \n");
    //fflush(stdout);
    WaitForSingleObject(&mutex, INFINITE);
    for (int i = 0; i < numberOfClients; i++) {
        shutdown(clients[i].socket, 2);
        closesocket(clients[i].socket);
        clients[i].socket = INVALID_SOCKET;
    }
    for (int i = 0; i < numberOfClients; i++) {
        WaitForSingleObject(clients[i].thread, INFINITE);
    }
    ReleaseMutex(&mutex);
    //printf("Listening ended \n");
    //fflush(stdout);
}

// обработчик одного клиента
DWORD WINAPI clientHandler(void *clientIndex) {
    WaitForSingleObject(&mutex, INFINITE);
    int index = *((int *) clientIndex);
    int socket = clients[index].socket;
    ReleaseMutex(&mutex);

    char message[MESSAGE_SIZE] = {0};
    int login_id = -1;
    // login password
    for (;;) {
        WaitForSingleObject(&mutex, INFINITE);
        for (;;) {
            memset(message, 0, sizeof(message));
            login_id = -1;
            readN(socket, message);
            for (int i = 0; i < num_login; i++) {
                if (!strcmp(clients_login[i].login, message)) {
                    login_id = i;
                    break;
                }
            }
            if (login_id == -1) {
                clients_login = (struct Client_Login *) realloc(clients_login,
                                                                sizeof(struct Client_Login) * (num_login + 1));
                memcpy(clients_login[num_login].login, message, sizeof(message));
                memcpy(clients[index].login, message, sizeof(message));
                login_id = num_login;

                memset(message, 0, sizeof(message));
                readN(socket, message);

                memcpy(clients_login[num_login].password, message, sizeof(message));
                num_login++;
                memset(message, 0, sizeof(message));
                message[0] = '+';
                send(socket, message, sizeof(message), 0);
                break;
            } else {
                memset(message, 0, sizeof(message));
                readN(socket, message);
                printf("%s", message);
                if (!strcmp(clients_login[login_id].password, message)) {
                    memcpy(clients[index].login, clients_login[login_id].login, sizeof(message));
                    memset(message, 0, sizeof(message));
                    message[0] = '+';
                    send(socket, message, sizeof(message), 0);
                    break;
                } else {
                    memset(message, 0, sizeof(message));
                    message[0] = '-';
                    send(socket, message, sizeof(message), 0);
                }
            }

        }
        ReleaseMutex(&mutex);
        int exit = 0;


        for (;;) {
            if (readN(socket, message) <= 0) {
                exit = 1;
                break;
            } else {
                printf("Received message: %s\n", message);
                fflush(stdout);

                if (!strcmp("-logout", message)) {
                    exit = 0;
                    char nolog[MESSAGE_SIZE] = {0};
                    memcpy(clients[index].login, nolog, sizeof(nolog));
                    send(socket, message, sizeof(message), 0);
                    break;
                }

                if (!strcmp("-who", message)) {
                    memset(message, 0, sizeof(message));
                    strcat(message, "\n");
                    for (int i = 0; i < numberOfClients; i++) {
                        char str[1000];
                        if (clients[i].socket != INVALID_SOCKET) {
                            sprintf(str, " %d          %s          %d          %s\n", clients[i].number,
                                    clients[i].address,
                                    clients[i].port, clients[i].login);
                            strcat(message, str);
                        }
                    }
                    Sleep(20000);

                }
                if (!strcmp("-ls", message)) {
                    struct _finddata_t f;
                    intptr_t hFile;

                    memset(message, 0, sizeof(message));
                    strcat(message, "\n");

                    if ((hFile = _findfirst("*.*", &f)) == -1L)
                        strcpy(message, "No files in current directory!\n");
                    else {
                        do {
                            char str[10000] = {0};
                            sprintf(str, " %-20s %10ld %30s \n",
                                    f.name, f.size, ctime(&f.time_write));
                            strcat(message, str);
                        } while (_findnext(hFile, &f) == 0);
                        _findclose(hFile);
                    }
                }
                if (!strcmp("-getdir", message)) {
                    memset(message, 0, sizeof(message));
                    char str[100] = {0};
                    getcwd(str, 100);
                    strcpy(message, str);
                }
                char msgcd[MESSAGE_SIZE] = {0};
                strcpy(msgcd, message);
                char *stringcd = strtok(msgcd, " ");

                if (!strcmp("-cd", stringcd)) {
                    stringcd = strtok(NULL, " ");
                    int result;
                    result = chdir(stringcd);
                    if (result != 0)
                        strcpy(message, "Can't change directory \n");
                    else strcpy(message, stringcd);
                }

                char msg[MESSAGE_SIZE] = {0};
                strcpy(msg, message);
                char *string = strtok(msg, " ");
                if (!strcmp("-kick", string)) {
                    string = strtok(NULL, " ");
                    int num = atoi(string);
                    if (string == NULL || (string[0] != '0' && num == 0)) {
                        memset(message, 0, sizeof(message));
                        strcpy(message, "Incorrect command. Enter'-kick No' \n");
                    } else {
                        if (clients[index].number == num) {
                            memset(message, 0, sizeof(message));
                            strcpy(message, "You can't kill yourself. Use command -quit \n");
                        } else {
                            kickClient(num);
                        }
                    }
                }
                send(socket, message, sizeof(message), 0);
            }
            // if cd, ls etc
            memset(message, 0, sizeof(message));
        }
        if (exit) break;
    }
    printf("Client %d disconnected \n", index);
    fflush(stdout);
    shutdown(socket, 2);
    closesocket(socket);
    clients[index].socket = INVALID_SOCKET;
    printf("Client %d handler thread terminated. \n", index);
    fflush(stdout);
    ExitThread(0);
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

void *kickClient(int number) {
    WaitForSingleObject(&mutex, INFINITE);
    for (int i = 0; i < numberOfClients; i++) {
        if (clients[i].number == number) {
            // сокет закрывается для чтения и для записи
            shutdown(clients[i].socket, 2);
            closesocket(clients[i].socket);
            clients[i].socket = INVALID_SOCKET;
            printf("Client %d terminated. \n", number);
            break;
        }
    }
    ReleaseMutex(&mutex);
}