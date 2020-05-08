#include <iostream>
#include <unistd.h>
#include <regex>
#include <cstdio>
#include <cstring>
#include <wait.h>
#include <algorithm>
#include <fcntl.h>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

int setOperation(char *operationText);

void signal_handler_CH(int signo);

void signal_handler_CONH(int signo);

bool checkFormat(char *input);

std::string getTime();

std::string elapsedTime(std::string startTime, std::string endTime);

int removeColon(std::string s);

void createSock();

double performOperation(int saveCurrentNumber, double currentTotal, int operation);

int killProcess(char *PID);

int killAllProcess();

int runProcess(char *processName, char *filePath);

int setOperationInput(char *operationText);

void updateClientList(int pid);

void *client(void *ptr);

void *connection(void *ptr);

void *inputHandler(void *ptr);

struct clients {
    int clientID = -1;
    int readingEnd = -1;
    int writingEnd = -1;
    int pid;
    int msgsock;
    std::string ip;
    std::string status;
};

std::string processList[20][6];
int currentListIndex = 0;
int activeProcesses = 0;
int write2CH[2];
int write2CON[2];
int currentClientIndex = -1;
int activeClients = 0;
int sock;
int msgsock;
bool divZero = false;
bool getFirstNumber;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

struct clients clientsList[10];

int main() {

    if (signal(SIGCHLD, signal_handler_CONH) == SIG_ERR) {
        write(STDOUT_FILENO, "sig error", 9);
    }
    pthread_t inputThread;
    int inputID;

    createSock();
    listen(sock, 5);
    inputID = pthread_create(&inputThread, nullptr, connection, (void *) nullptr);
    struct sockaddr_in addr;
    socklen_t client_addr_size = sizeof(struct sockaddr_in);
    int clientHandlerPID;

    while (true) {
        msgsock = accept(sock, (struct sockaddr *) &addr, &client_addr_size);
        if (msgsock != -1) {
            currentClientIndex++;
            activeClients++;
            int checkPipe = pipe(write2CH);
            int checkPipe2 = pipe(write2CON);

            clientsList[currentClientIndex].clientID = currentClientIndex + 1;
            clientsList[currentClientIndex].readingEnd = write2CON[0];
            clientsList[currentClientIndex].writingEnd = write2CH[1];
            clientsList[currentClientIndex].ip = inet_ntoa(addr.sin_addr);
            clientsList[currentClientIndex].msgsock = msgsock;
            clientsList[currentClientIndex].status = "Connected";

            clientHandlerPID = fork();

            if (clientHandlerPID == 0) {
                pthread_t clientHThread;
                pthread_t inputHThread;
                int clientHID;
                int inputHID;

                clientHID = pthread_create(&clientHThread, nullptr, client, (void *) nullptr);
                inputHID = pthread_create(&inputHThread, nullptr, inputHandler, (void *) nullptr);

                pthread_join(clientHThread, nullptr);
                pthread_join(inputHThread, nullptr);
            }
        } else {
            write(STDOUT_FILENO, "Connection fail\n", 16);
        }
        clientsList[currentClientIndex].pid = clientHandlerPID;
    }
    pthread_join(inputThread, nullptr);
    return 0;
}

#pragma clang diagnostic pop

//creating socket
void createSock() {
    char output[500];
    struct sockaddr_in server;//struct to store socket info
    int length;
    sock = socket(AF_INET, SOCK_STREAM, 0);//socket created
    if (sock < 0) {
        perror("opening stream socket");
        exit(1);
    } else {//setting values in the structure
        server.sin_family = AF_INET;//for communication over the internet
        server.sin_addr.s_addr = INADDR_ANY;//can connect to any address
        server.sin_port = 0;//passing 0 so system can assign any port number
    }
    if (bind(sock, (struct sockaddr *) &server, sizeof(server))) {//binding socket with the port
        perror("binding stream socket");
        exit(1);
    }
    length = sizeof(server);
    if (getsockname(sock, (struct sockaddr *) &server, (socklen_t *) &length)) {// getting the assigned port
        perror("getting socket name");
        exit(1);
    }
    int portNo = ntohs(server.sin_port);
    int noOfChars = sprintf(output, "%s", "Socket has port #\n");
    int portChars = sprintf(&output[noOfChars - 1], "%d\n", portNo);
    int count = noOfChars + portChars;
    write(STDOUT_FILENO, output, count);
    fflush(stdout);
}

//run processes on client's command
int runProcess(char *processName, char *filePath) {
    int pipefds3[2];
    int pipeCheck = pipe2(pipefds3, O_CLOEXEC);
    int pidChild2 = fork();
    if (pidChild2 == 0) {
        close(pipefds3[0]);
        int execCheck = execlp(processName, processName, filePath, NULL);
        if (execCheck == -1) {
            if (errno == EACCES) {
                write(pipefds3[1], "User does not have access right for the file\n", 46);
            } else if (errno == EFAULT) {
                write(pipefds3[1], "File outside user accessible memory\n", 36);
            } else if (errno == ENOENT) {
                write(pipefds3[1], "File does not exist\n", 20);
            } else {
                write(pipefds3[1], "Error\n", 6);
            }
            kill(getpid(), SIGTERM);
        }
    }
    if (pidChild2 > 0) {
        close(pipefds3[1]);
        char errorCheck[50];
        processList[currentListIndex][0] = std::to_string(pidChild2);
        processList[currentListIndex][1] = processName;
        processList[currentListIndex][2] = "Running";
        processList[currentListIndex][3] = getTime();
        processList[currentListIndex][4] = "-";
        processList[currentListIndex][5] = "-";
        activeProcesses++;
        currentListIndex++;
        bool error = false;
        int readCheck = read(pipefds3[0], errorCheck, 50);
        if (readCheck > 0) {
            error = true;
        }
        if (error) {
            int noOfChars = sprintf(&errorCheck[readCheck - 1], "%s", "\nInput next command\n");
            activeProcesses--;
            currentListIndex--;
            int count = readCheck + noOfChars;
            write(msgsock, errorCheck, count);
        } else {
            write(msgsock, "Input next command\n", 19);
        }
    }
    return 0;
}

//method to get current time
std::string getTime() {
    time_t rawtime;
    struct tm *timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
    std::string str(buffer);
    return str;
}

//helper method for time calculations
int removeColon(std::string s) {
    char buffer[8];
    int count = sprintf(buffer, "%s", s.c_str());
    std::replace(s.begin(), s.end(), ':', '0');
    count = sprintf(buffer, "%s", s.c_str());
    return std::stoi(s);
}

//calculating elapsed time
std::string elapsedTime(std::string startTime, std::string endTime) {
    int time1 = removeColon(startTime);
    int time2 = removeColon(endTime);

    int hourDiff = time2 / 1000000 - time1 / 1000000 - 1;
    time1 = time1 % 1000000;
    time2 = time2 % 1000000;

    int minDiff = time2 / 1000 + (60 - time1 / 1000);
    if (minDiff >= 60) {
        hourDiff++;
        minDiff = minDiff - 60;
    }

    time1 = time1 % 100;
    time2 = time2 % 100;

    int secDiff = (time2) + (60 - time1);
    if (secDiff >= 60) {
        secDiff = secDiff - 60;
    }
    std::string res = std::to_string(hourDiff) + ':' + std::to_string(minDiff) + ':' + std::to_string(secDiff);
    return res;
}

//kill process by pid or name
int killProcess(char *PID) {
    bool pidFound = false;
    bool nameFound = false;
    int index = -1;
    for (int i = 0; i < currentListIndex; ++i) {
        if (strcmp(PID, processList[i][0].c_str()) == 0) {
            index = i;
            pidFound = true;
            break;
        }
    }
    for (int process = 0; process < currentListIndex; ++process) {
        if (strcmp(PID, processList[process][1].c_str()) == 0 &&
            strcmp(processList[process][2].c_str(), "Running") == 0) {
            index = process;
            nameFound = true;
            break;
        }
    }
    if (pidFound || nameFound) {
        int pid = atoi(processList[index][0].c_str());
        if (index != -1 && strcmp(processList[index][2].c_str(), "Running") == 0) {
            kill(pid, SIGTERM);
            processList[index][2] = "Killed";
            processList[index][4] = getTime();
            processList[index][5] = elapsedTime(processList[index][3], processList[index][4]);
            activeProcesses--;
            return 0;
        } else if (index != -1 && strcmp(processList[index][2].c_str(), "Killed") == 0) {
            return -1;
        } else if (index != -1 && strcmp(processList[index][2].c_str(), "Extrl Termination") == 0) {
            return -1;
        }
    } else {
        return 1;
    }
}

//killing all client processes at exit
int killAllProcess() {
    for (int i = 0; i < currentListIndex; ++i) {
        int pid = atoi(processList[i][0].c_str());
        if (strcmp(processList[i][2].c_str(), "Running") == 0) {
            kill(pid, SIGTERM);
            processList[i][2] = "Killed";
            processList[i][4] = getTime();
            processList[i][5] = elapsedTime(processList[i][3], processList[i][4]);
            activeProcesses--;
            return 0;
        }
    }
}

//method for calculations
double performOperation(int saveCurrentNumber, double currentTotal, int operation) {
    if (!getFirstNumber) {
        currentTotal = saveCurrentNumber;
        getFirstNumber = true;
    } else {
        switch (operation) {
            case 1:
                currentTotal = currentTotal + saveCurrentNumber;
                break;
            case 2:
                currentTotal = currentTotal - saveCurrentNumber;
                break;
            case 3:
                currentTotal = currentTotal * saveCurrentNumber;
                break;
            case 4:
                if (saveCurrentNumber == 0) {
                    divZero = true;
                } else {
                    currentTotal = currentTotal / saveCurrentNumber;
                }
                break;
        }
    }
    return currentTotal;
}

//operation setter for client handler
int setOperation(char *operationText) {
    int operation;
    if (strcmp(operationText, "add") == 0) {
        operation = 1;
    } else if (strcmp(operationText, "sub") == 0) {
        operation = 2;
    } else if (strcmp(operationText, "mul") == 0) {
        operation = 3;
    } else if (strcmp(operationText, "div") == 0) {
        operation = 4;
    } else if (strcmp(operationText, "kill") == 0) {
        operation = 5;
    } else if (strcmp(operationText, "run") == 0) {
        operation = 6;
    } else if (strcmp(operationText, "list") == 0) {
        operation = 7;
    } else if (strcmp(operationText, "print") == 0) {
        operation = 8;
    } else if (strcmp(operationText, "exit") == 0) {
        operation = 0;
    } else {
        operation = -1;
    }
    return operation;
}

//operation setter for input handler and input thread
int setOperationInput(char *operationText) {
    int operation;
    if (strcmp(operationText, "print2all") == 0) {
        operation = 1;
    } else if (strcmp(operationText, "list") == 0) {
        operation = 2;
    } else if (strcmp(operationText, "print") == 0) {
        operation = 3;
    } else {
        operation = -1;
    }
    return operation;
}

//checking int for calculations
bool checkFormat(char *input) {
    bool isNumber;
    std::regex b("^[-+]?\\d+$");//accepts negative and positive integers only
    if (regex_match(input, b)) {
        isNumber = true;
    } else {
        isNumber = false;
    }
    return isNumber;
}

//for handling client run processes exit
void signal_handler_CH(int signo) {
    if (signo == SIGCHLD) {
        int status;
        for (int activeProcess = 1; activeProcess <= activeProcesses; ++activeProcess) {
            int pid = waitpid(0, &status, WNOHANG);
            if (pid != 0) {
                int index = -1;
                for (int i = 0; i < currentListIndex; ++i) {
                    if (pid == stoi(processList[i][0])) {
                        index = i;
                        break;
                    }
                }
                if (index != -1 && strcmp(processList[index][2].c_str(), "Running") == 0) {
                    processList[index][2] = "Extrl Termination";
                    processList[index][4] = getTime();
                    processList[index][5] = elapsedTime(processList[index][3], processList[index][4]);
                    activeProcesses--;
                }
            }
        }
    }
}

//for handling client handler exits
void signal_handler_CONH(int signo) {
    if (signo == SIGCHLD) {
        int status;
        for (int activeClient = 0; activeClient <= activeClients; ++activeClient) {
            int pid = waitpid(0, &status, WNOHANG);
            if (pid != 0) {
                updateClientList(pid);
            }
        }
    }
}

//client handler thread
void *client(void *ptr) {
    char inputText[500];
    char outputText[500];
    bool continueInput = true;
    char saveOperator[10];
    int operation = -1;
    char *token;


    if (signal(SIGCHLD, signal_handler_CH) == SIG_ERR) {
        write(STDOUT_FILENO, "sig error", 9);
    }

    write(msgsock, "Commands: kill <pid>, list, run <process> <path(optional)>, "
                   "add/div/sub/mul <list of numbers separated by spaces>\nInput exit to terminate:\n"
                   "Please input your command:\n", 166);

    while (continueInput) {
        int checkRead = read(msgsock, inputText, 500);//reading from socket
        inputText[checkRead - 1] = '\0';//adding null at the end

        if (checkRead == 1) {//empty input
            write(msgsock, "Input next command\n", 19);
            continue;
        }

        //getting the first token to set operation
        token = strtok(inputText, " ");
        sscanf(token, "%s", saveOperator);
        operation = setOperation(saveOperator);

        //exit
        if (operation == 0) {
            continueInput = false;
            int *status = nullptr;
            write(msgsock, "exit\0", 5);
            close(sock);
            close(msgsock);
            killAllProcess();
            wait(status);
            kill(getpid(), SIGTERM);
        }

            //invalid input
        else if (operation == -1) {
            write(msgsock, "Invalid command.\nInput next command\n", 36);
        }

            //add/sub/div/mul
        else if (operation >= 1 && operation <= 4) {
            int saveCurrentNumber;
            bool invalidInput = false;
            double total = 0;
            getFirstNumber = false;
            char checkInteger[10];
            token = strtok(nullptr, " ");

            //calculation loop
            while (token != nullptr) {
                sscanf(token, "%d", &saveCurrentNumber);
                sscanf(token, "%s", checkInteger);
                if (checkFormat(checkInteger)) {
                    total = performOperation(saveCurrentNumber, total, operation);
                } else {
                    invalidInput = true;
                }
                token = strtok(nullptr, " ");
            }

            //printing logic
            int noOfCharPrint = sprintf(outputText, "%.5f \n", total);
            int count;
            if (invalidInput) {
                int noOfChars = sprintf(&outputText[noOfCharPrint - 1], "%s",
                                        "\nOnly Integer values considered for calculations. Others were ignored.\nInput next command\n");
                count = noOfCharPrint + noOfChars;
            } else if (divZero) {
                int noOfChars = sprintf(&outputText[noOfCharPrint - 1], "%s",
                                        "\nDivision by Zero. Invalid Operation.\nInput next command\n");
                count = noOfCharPrint + noOfChars;
            } else {
                int noOfChars = sprintf(&outputText[noOfCharPrint - 1], "%s", "\nInput next command\n");
                count = noOfCharPrint + noOfChars;
            }
            write(msgsock, outputText, count);
        }

            //kill
        else if (operation == 5) {
            char *processPID;
            processPID = strtok(nullptr, " ");
            if (processPID == nullptr) {
                write(msgsock, "Input next command\n", 19);
            } else {
                int killCheck = killProcess(processPID);
                if (killCheck == 1) {
                    write(msgsock, "Invalid pid\nInput next command\n", 31);
                } else if (killCheck == 0) {
                    write(msgsock, "Process killed\nInput next command\n", 34);
                } else {
                    write(msgsock, "Process already killed.\nInput next command\n", 43);
                }
            }
        }

            //run
        else if (operation == 6) {
            char *processName;
            processName = strtok(nullptr, " ");
            char *filePath;
            filePath = strtok(nullptr, " ");
            if (processName != nullptr) {
                runProcess(processName, filePath);
            } else {
                write(msgsock, "Input next command\n", 19);
            }
        }

            //list
        else if (operation == 7) {
            char output[500];
            std::string print;
            print.append("Process PID\tProcess Name\tStatus\t\tStart Time\t\tEnd Time\t\tElapsed Time\n\n");
            for (int i = 0; i < currentListIndex; ++i) {
                for (int j = 0; j < 6; ++j) {
                    print.append(processList[i][j]).append("\t\t");
                }
                print.append("\n");
            }
            int read = sprintf(output, "%s", print.c_str());
            sprintf(&output[read - 1], "%s", "\nInput next command\n");
            int count = read + 20;
            write(msgsock, output, count);
        }

            //print
        else if (operation == 8) {
            token = strtok(nullptr, " ");
            char messageBuffer[500];
            std::string print;
            print.append("Message from client: ");
            while (token != nullptr) {
                print.append(token).append(" ");
                token = strtok(nullptr, " ");
            }
            print.append("\n");
            int read = sprintf(messageBuffer, "%s", print.c_str());
            write(STDOUT_FILENO, messageBuffer, read);
            write(msgsock, "Input next command\n", 19);
        }
    }
}

//Input Thread
void *connection(void *ptr) {
    char input[500];
    char saveOperator[10];
    int operation = -1;
    char *token;
    int checkRead;

    while (true) {
        write(STDIN_FILENO, "Input next command\n", 19);
        checkRead = read(STDIN_FILENO, input, 500);//B1
        input[checkRead - 1] = '\0';//adding null at the end

        if (checkRead == 1) {//empty input
            write(STDIN_FILENO, "No command input\n", 17);
        } else {
            //getting the first token to set operation
            token = strtok(input, " ");
            sscanf(token, "%s", saveOperator);
            operation = setOperationInput(saveOperator);

            if (operation == -1) {
                write(STDOUT_FILENO, "Invalid Command\n", 16);
            } else if (operation == 1) {
                if (activeClients == 0) {
                    write(STDOUT_FILENO, "No Client Connected\n", 20);
                } else {
                    char output[500];
                    std::string print;
                    print.append("print2all Message from server: ");
                    token = strtok(nullptr, " ");
                    while (token != nullptr) {
                        print.append(token).append(" ");
                        token = strtok(nullptr, " ");
                    }
                    print.append("\n");
                    int count = sprintf(output, "%s", print.c_str());
                    for (int i = 0; i <= currentClientIndex; ++i) {
                        if (strcmp(clientsList[i].status.c_str(), "Connected") == 0) {
                            int checkWrite = write(clientsList[i].writingEnd, output, count);
                        }
                    }
                }
            } else if (operation == 2) {
                char output[1000];
                std::string print;
                int currentPosition = 0;
                if (activeClients == 0) {
                    write(STDOUT_FILENO, "No Client Connected\n", 20);
                } else {
                    print.append("Process PID\tProcess Name\tStatus\t\tStart Time\t\tEnd Time\t\tElapsed Time\n\n");
                    for (int i = 0; i <= currentClientIndex; i++) {
                        if (strcmp(clientsList[i].status.c_str(), "Connected") == 0) {
                            int checkWrite = write(clientsList[i].writingEnd, "list ", 5);
                            int count = read(clientsList[i].readingEnd, input, 500);//B3
                            sprintf(&output[currentPosition], "%s", input);
                            currentPosition = currentPosition + count;
                        }
                    }
                    write(STDOUT_FILENO, output, currentPosition);
                }
            } else if (operation == 3) {
                if (activeClients == 0) {
                    write(STDOUT_FILENO, "No Client Connected\n", 20);
                } else {
                    char output[500];
                    char saveIP[100];
                    std::string print;
                    char buf[sizeof(struct in6_addr)];
                    int clientIndex;
                    int ipCheck = -1;
                    token = strtok(nullptr, " ");
                    if (token == nullptr) {
                        write(STDOUT_FILENO, "No IP Provided\n", 15);
                    } else {
                        ipCheck = inet_pton(AF_INET, token, buf);
                        if (ipCheck <= 0) {
                            write(STDOUT_FILENO, "No Client Connected\n", 20);
                        } else {
                            for (int i = 0; i <= currentClientIndex; ++i) {
                                sscanf(clientsList[i].ip.c_str(), "%s", saveIP);
                                if (strcmp(saveIP, token) == 0) {
                                    clientIndex = i;
                                }
                            }
                            if (ipCheck != -1) {
                                print.append("print Message from server: ");
                                token = strtok(nullptr, " ");
                                while (token != nullptr) {
                                    print.append(token).append(" ");
                                    token = strtok(nullptr, " ");
                                }
                                print.append("\n");
                                int count = sprintf(output, "%s", print.c_str());
                                if (strcmp(clientsList[clientIndex].status.c_str(), "Connected") == 0) {
                                    int checkWrite = write(clientsList[clientIndex].writingEnd, output, count);
                                }
                            } else {
                                write(STDOUT_FILENO, "IP does not exist\n", 18);
                            }
                        }
                    }
                }
            }
        }
    }
}

//update client list on disconnects
void updateClientList(int pid) {
    int index = -1;
    for (int client = 0; client <= currentClientIndex; ++client) {
        if (clientsList[client].pid == pid) {
            index = client;
            break;
        }
    }
    if (index != -1) {
        close(clientsList[index].writingEnd);
        close(clientsList[index].readingEnd);
        close(clientsList[index].msgsock);
        clientsList[index].status = "Disconnected";
        activeClients--;
    }
}

//Input handler thread
void *inputHandler(void *ptr) {
    char input[500];
    char output[500];
    char saveOperator[10];
    int operation = -1;
    char *token;
    int checkRead;

    while (true) {
        checkRead = read(write2CH[0], input, 500);//B2//B4
        input[checkRead - 1] = '\0';//adding null at the end

        token = strtok(input, " ");
        sscanf(token, "%s", saveOperator);
        operation = setOperationInput(saveOperator);

        if (operation == 1 || operation == 3) {
            std::string print;
            token = strtok(nullptr, " ");
            while (token != nullptr) {
                print.append(token).append(" ");
                token = strtok(nullptr, " ");
            }
            print.append("\n");
            int count = sprintf(output, "%s", print.c_str());
            write(msgsock, output, count);
        } else if (operation == 2) {
            std::string print;
            print.append("Process PID\tProcess Name\tStatus\t\tStart Time\t\tEnd Time\t\tElapsed Time\n\n");
            for (int i = 0; i < currentListIndex; ++i) {
                for (int j = 0; j < 6; ++j) {
                    print.append(processList[i][j]).append("\t\t");
                }
                print.append("\n");
            }
            int count = sprintf(output, "%s", print.c_str());
            write(write2CON[1], output, count);
        }
    }
}