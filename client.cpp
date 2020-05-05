#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstring>

void createSock(char *ip, char *port);

void *getResult(void *ptr);

void *getCommand(void *ptr);

int sock;
char inputText[500];
char outputText[500];
bool continueInput = true;

int main(int argc, char *argv[]) {
    char *ip = argv[1];
    char *port = argv[2];

    pthread_t GCThread, GRThread;
    int tid1, tid2;

    createSock(ip, port);

    int readCheck = read(sock, outputText, 500);
    write(STDOUT_FILENO, outputText, readCheck);


    tid1 = pthread_create(&GRThread, nullptr, getResult, (void *) nullptr);
    if (tid1) {
        write(STDOUT_FILENO, "Error on read thread\n", 21);
        exit(EXIT_FAILURE);
    }


    tid2 = pthread_create(&GCThread, nullptr, getCommand, (void *) nullptr);
    if (tid2) {
        write(STDOUT_FILENO, "Error on read thread\n", 21);
        exit(EXIT_FAILURE);
    }

    pthread_join(GCThread, NULL);

    pthread_join(GRThread, NULL);
}

void createSock(char *ip, char *port) {
    struct sockaddr_in server;
    struct hostent *hp;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("opening stream socket");
        exit(1);
    }
    /* Connect socket using name specified by command line. */
    server.sin_family = AF_INET;
    hp = gethostbyname(ip);
    if (hp == 0) {
        fprintf(stderr, "%s: unknown host\n", ip);
        exit(2);
    }
    bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
    server.sin_port = htons(atoi(port));
    if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("connecting stream socket");
        exit(1);
    } else {
        write(STDOUT_FILENO, "connection established\n", 23);
    }
}

void *getCommand(void *ptr) {
    while (true) {
        int readCheck = read(STDIN_FILENO, inputText, 500);
        int writeCheck = write(sock, inputText, readCheck);
    }
}

void *getResult(void *ptr) {
    while (true) {
        int readCheck = read(sock, outputText, 500);
        if (strcmp(outputText, "exit") == 0) {

            close(sock);
            exit(0);
        } else {
            write(STDOUT_FILENO, outputText, readCheck);
        }
    }
}