#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "env.h"
#include "chat.pb-c.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

int cli_socket_descript = 0;
int is_connected = 0;
char cli_name[MAX_USERNAME_LENGTH] = {};

void exit_service(int signal) {
    printf("\nShutting down...\n");
    is_connected = 0;
    exit(EXIT_SUCCESS);
}

void create_user_action(){
    // Prepare a petition to set the username
    Chat__NewUserRequest new_user_request = CHAT__NEW_USER_REQUEST__INIT;
    new_user_request.username = cli_name;

    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__REGISTER_USER;
    request.payload_case = CHAT__REQUEST__PAYLOAD_REGISTER_USER;
    request.register_user = &new_user_request;

    // Serialize the request
    size_t req_len = chat__request__get_packed_size(&request);
    void *req_buffer = malloc(req_len);
    if (req_buffer == NULL) {
        printf("Memory allocation failed!\n");
        exit(EXIT_FAILURE);
    }
    chat__request__pack(&request, req_buffer);

    // Send the request
    int bytes_sent = send(cli_socket_descript, req_buffer, req_len, 0);
    if(bytes_sent<0){
        printf("Send failed!\n");
        exit(EXIT_FAILURE);
    }

    char res_buffer[BUFFER_SIZE];
    int res = recv(cli_socket_descript, res_buffer, BUFFER_SIZE, 0);
    if (res < 0) {
        printf("Receive failed!\n");
        exit(EXIT_FAILURE);
    }

    Chat__Response *response = chat__response__unpack(NULL, res, res_buffer);
    if (response == NULL) {
        printf("Error unpacking response\n");
        exit(EXIT_FAILURE);
    }

    if (response->status_code == CHAT__STATUS_CODE__OK) {
        printf("Message: %s\n", response->message);
    } else {
        printf("Error: %s\n", response->message);
        exit(EXIT_FAILURE);
    }
}

char* parse_user_status(int status){
    switch (status){
        case CHAT__USER_STATUS__ONLINE:
            return "Active";
        case CHAT__USER_STATUS__BUSY:
            return "Inactive";
        case CHAT__USER_STATUS__OFFLINE:
            return "Away";
        default:
            return "Unknown";
    }
}

void get_all_users_action(char* username){
    if (strlen(username) > 0){
        printf("Getting user %s...\n", username);
        Chat__UserListRequest user_list_request = CHAT__USER_LIST_REQUEST__INIT;
        user_list_request.username = username;

        Chat__Request request = CHAT__REQUEST__INIT;
        request.operation = CHAT__OPERATION__GET_USERS;
        request.payload_case = CHAT__REQUEST__PAYLOAD_GET_USERS;
        request.get_users = &user_list_request;

        // Serialize the request
        size_t req_len = chat__request__get_packed_size(&request);
        void *req_buffer = malloc(req_len);
        if (req_buffer == NULL) {
            printf("Memory allocation failed!\n");
            exit(EXIT_FAILURE);
        }

        chat__request__pack(&request, req_buffer);

        // Send the request
        int bytes_sent = send(cli_socket_descript, req_buffer, req_len, 0);
        if(bytes_sent<0){
            printf("Send failed!\n");
            exit(EXIT_FAILURE);
        }

        char res_buffer[BUFFER_SIZE];
        int res = recv(cli_socket_descript, res_buffer, BUFFER_SIZE, 0);
        if (res < 0) {
            printf("Receive failed!\n");
            exit(EXIT_FAILURE);
        } 

        Chat__Response *response = chat__response__unpack(NULL, res, res_buffer);
        if (response == NULL) {
            printf("Error unpacking response\n");
            exit(EXIT_FAILURE);
        }

        if (response->status_code == CHAT__STATUS_CODE__OK) {
            printf("\nMessage: %s\n", response->message);
            printf("\n");
            printf("Username: %s\n", response->user_list->users[0]->username);
            printf("Status: %s\n", parse_user_status(response->user_list->users[0]->status));
        } else {
            printf("Error: %s\n", response->message);
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Getting all users...\n");
        // Prepare a petition to get all users
        Chat__UserListRequest user_list_request = CHAT__USER_LIST_REQUEST__INIT;
        user_list_request.username = username;

        Chat__Request request = CHAT__REQUEST__INIT;
        request.operation = CHAT__OPERATION__GET_USERS;
        request.payload_case = CHAT__REQUEST__PAYLOAD_GET_USERS;
        request.get_users = &user_list_request;


        // Serialize the request
        size_t req_len = chat__request__get_packed_size(&request);
        void *req_buffer = malloc(req_len);
        if (req_buffer == NULL) {
            printf("Memory allocation failed!\n");
            exit(EXIT_FAILURE);
        }
        chat__request__pack(&request, req_buffer);

        // Send the request
        int bytes_sent = send(cli_socket_descript, req_buffer, req_len, 0);
        if(bytes_sent<0){
            printf("Send failed!\n");
            exit(EXIT_FAILURE);
        }

        char res_buffer[BUFFER_SIZE];
        int res = recv(cli_socket_descript, res_buffer, BUFFER_SIZE, 0);
        if (res < 0) {
            printf("Receive failed!\n");
            exit(EXIT_FAILURE);
        }

        Chat__Response *response = chat__response__unpack(NULL, res, res_buffer);
        if (response == NULL) {
            printf("Error unpacking response\n");
            exit(EXIT_FAILURE);
        }

        if (response->status_code == CHAT__STATUS_CODE__OK) {
            printf("\nMessage: %s\n", response->message);
            for (int i = 0; i < response->user_list->n_users; i++){
                printf("\n");
                printf("Username: %s\n", response->user_list->users[i]->username);
                printf("Status: %s\n", parse_user_status(response->user_list->users[i]->status));
            }
        } else {
            printf("Error: %s\n", response->message);
            exit(EXIT_FAILURE);
        }
    }
}

/*
* Main function
* @param argc: number of arguments
* @param argv: arguments
* @return: 0 if successful, 1 if failed
* Based of https://www.geeksforgeeks.org/tcp-server-client-implementation-in-c/
*/
int main(int argc, char *argv[]){
    if (argc != 4) {
        printf("Usage: %s <ip> <port> <name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char* ip = argv[1];
    int port = atoi(argv[2]);
    char* name = argv[3];

    strcpy(cli_name, name);
    // Check if the name is valid
    if (strlen(name) < 1 || strlen(name) > MAX_USERNAME_LENGTH){
        printf("Invalid name!\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, exit_service);

    cli_socket_descript = socket(AF_INET, SOCK_STREAM, 0);
    if (cli_socket_descript == -1) {
        printf("Socket creation failed!\n");
        exit(EXIT_FAILURE);
    } else{
        printf("Socket created successfully!\n");
    }

    // Save the server address and client address
    struct sockaddr_in srv_address, cli_address;
    int srv_addr_len = sizeof(srv_address);
    int cli_addr_len = sizeof(cli_address);

    // Initialize the server and client address
    memset(&srv_address, 0, srv_addr_len);
    memset(&cli_address, 0, cli_addr_len);

    // Set the server address and port
    srv_address.sin_family = AF_INET;
    srv_address.sin_port = htons(port);
    srv_address.sin_addr.s_addr = inet_addr(ip);

    // Connect to the server
    if (connect(cli_socket_descript, (struct sockaddr *) &srv_address, srv_addr_len) == -1) {
        printf("Connection failed!\n");
        exit(EXIT_FAILURE);
    } else {
        // Show server information
        getsockname(cli_socket_descript, (struct sockaddr *) &cli_address, &cli_addr_len);
        getpeername(cli_socket_descript, (struct sockaddr *) &srv_address, &srv_addr_len);
        printf("Connection successful! You are connected to %s:%d\n", inet_ntoa(srv_address.sin_addr), ntohs(srv_address.sin_port));
        printf("Your IP address is %s and your port is %d\n", inet_ntoa(cli_address.sin_addr), ntohs(cli_address.sin_port));
        is_connected = 1;
    }

    create_user_action();

    while (is_connected){
        printf("\nSelect an option:\n");
        printf("1. Send a message\n");
        printf("2. List users\n");
        printf("3. Change channel\n");
        printf("4. Change status\n");
        printf("5. Exit\n");

        int option;
        scanf("%d", &option);

        switch (option){
            case 1:
                break;
            case 2:
                printf("Do you want to get all users? (y/n)\n");
                char answer;
                scanf(" %c", &answer);
                if (answer == 'y'){
                    get_all_users_action("");
                } else {
                    printf("Type the username if you want to get an specific user\n");
                    char username[MAX_USERNAME_LENGTH];
                    scanf("%s", username);
                    get_all_users_action(username);
                }
                break;
            case 3:
                break;
            case 4:
                break;
            case 5:
                is_connected = 0;
                printf("Shutting down connection...\n");
                printf("Goodbye!\n");
                break;
            default:
                printf("Invalid option!\n");
                break;
        }

    };
    
    close(cli_socket_descript);
    return 0;
}