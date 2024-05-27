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
Chat__MessageType channel = CHAT__MESSAGE_TYPE__BROADCAST;
char current_chat[MAX_USERNAME_LENGTH] = {};
int cli_status = CHAT__USER_STATUS__OFFLINE;

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
            return "Busy";
        case CHAT__USER_STATUS__OFFLINE:
            return "Inactivo";
        default:
            return "Unknown";
    }
}

void *message_listener(void * arg){
    pthread_detach(pthread_self());
    while (is_connected){
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
            if (response->operation == CHAT__OPERATION__INCOMING_MESSAGE){
                if (response->incoming_message->type == CHAT__MESSAGE_TYPE__BROADCAST){
                    printf("\n\033[0;35mGLOBAL\033[0m - Message from %s: %s\n\n", response->incoming_message->sender, response->incoming_message->content);
                } else {
                    printf("\n\033[0;34mPRIVATE\033[0m - Message from %s: %s\n\n", response->incoming_message->sender, response->incoming_message->content);
                }
            } 

            if (response->operation == CHAT__OPERATION__SEND_MESSAGE){
                if (strlen(response->message) > 0){
                    printf("%s\n", response->message);
                } 
            }

            if (response->operation == CHAT__OPERATION__UPDATE_STATUS){
                if (strlen(response->message) > 0){
                    printf("%s\n", response->message);
                } 
            }
            
        } else {
            if (strlen(response->message) > 0){
                printf("Error: %s\n", response->message);
            } else {
                printf("Server disconnected!\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}

int check_user_status(char* username){
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
        return response->user_list->users[0]->status;
        if (response->user_list->users[0]->status == CHAT__USER_STATUS__ONLINE){
            return 1;
        } else if (response->user_list->users[0]->status == CHAT__USER_STATUS__BUSY){
            return 2;
        } else {
            return 3;
        }
    } else {
        printf("Error: %s\n", response->message);
        return -1;
    }
}

char* get_all_users_action(char* username){
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

        printf("Received!\n");

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
            return "";
        }

        return response->user_list->users[0]->username;

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
    return "";
}

void send_message_action(char* message){
    Chat__SendMessageRequest send_message_request = CHAT__SEND_MESSAGE_REQUEST__INIT;
    send_message_request.content = message;
    if (channel == CHAT__MESSAGE_TYPE__BROADCAST){
        send_message_request.recipient = "";
    } else {
        send_message_request.recipient = current_chat;
    }

    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__SEND_MESSAGE;
    request.payload_case = CHAT__REQUEST__PAYLOAD_SEND_MESSAGE;
    request.send_message = &send_message_request;

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
}

void change_status_action (Chat__UserStatus status){
    Chat__UpdateStatusRequest change_status_request = CHAT__UPDATE_STATUS_REQUEST__INIT;
    change_status_request.new_status = status;
    change_status_request.username = cli_name;

    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__UPDATE_STATUS;
    request.payload_case = CHAT__REQUEST__PAYLOAD_UPDATE_STATUS;
    request.update_status = &change_status_request;

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

    cli_status = status;

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

void client_disconnect_action() {
    Chat__User user_payload = CHAT__USER__INIT;
    user_payload.username = cli_name;
    user_payload.status = CHAT__USER_STATUS__OFFLINE;

    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__UNREGISTER_USER;
    request.payload_case = CHAT__REQUEST__PAYLOAD_UNREGISTER_USER;
    request.unregister_user = &user_payload;

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
}

void view_and_change_status() {
    // Display current status
    printf("Your current status is: %s\n", parse_user_status(cli_status));

    // Ask if user wants to change status
    printf("Do you want to change your status? (y/n)\n");
    char choice;
    scanf(" %c", &choice);

    if (choice == 'y' || choice == 'Y') {
        printf("Select your new status:\n");
        printf("1. Online\n");
        printf("2. Busy\n");
        printf("3. Offline\n");
        printf("Enter your choice: ");
        int new_status_choice;
        scanf("%d", &new_status_choice);

        Chat__UserStatus new_status;
        switch (new_status_choice) {
            case 1:
                new_status = CHAT__USER_STATUS__ONLINE;
                break;
            case 2:
                new_status = CHAT__USER_STATUS__BUSY;
                break;
            case 3:
                new_status = CHAT__USER_STATUS__OFFLINE;
                break;
            default:
                printf("Invalid choice, no changes made.\n");
                return;
        }

        change_status_action(new_status);
        printf("Status updated successfully!\n");
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

    // Main loop
    while (is_connected){
        if (channel == CHAT__MESSAGE_TYPE__BROADCAST){
            printf("\nYou are in the \033[0;35mGLOBAL\033[0m channel\n");
        } else {
            printf("\nYou are in the \033[0;34mPRIVATE\033[0m channel with %s\n", current_chat);
        }
        printf("\nSelect an option:\n");
        printf("1. Join chatroom\n");
        printf("2. User's guide\n");
        printf("3. List users\n");
        printf("4. Change channel\n");
        printf("5. View and change my status\n");
        printf("6. Exit\n");

        int option;
        scanf("%d", &option);

        switch (option){
            case 1:
                change_status_action(CHAT__USER_STATUS__ONLINE);
                printf("Welcome to the chatroom! Your status is now \033[0;32mONLINE\033[0m\n");
                printf("You are sending messages to the %s channel\n", channel == CHAT__MESSAGE_TYPE__BROADCAST ? "\033[0;35mGLOBAL\033[0m" : "\033[0;34mPRIVATE\033[0m");
                printf("You can leave the chatroom by typing '--exit'\n");
                pthread_t listener_thread;
                pthread_create(&listener_thread, NULL, message_listener, NULL);
                if (pthread_detach(listener_thread) != 0) {
                    printf("Thread creation failed!\n");
                    continue;
                }
                printf("Type your messages:\n");
                char message[MAX_MESSAGE_LENGTH];
                while (fgets(message, MAX_MESSAGE_LENGTH, stdin)) {
                    
                    message[strcspn(message, "\n")] = 0;
                    if (strcmp(message, "--exit") == 0){
                        break;
                    }
                    if (strlen(message) > 0){
                        send_message_action(message);
                    }
                    
                }
                printf("Leaving chatroom...\n");
                change_status_action(CHAT__USER_STATUS__OFFLINE);
                printf("Your status is now \033[0;31mOFFLINE\033[0m\n");
                break;
            case 2:
                printf("-----------------------------------------------------------------------------\n");
                printf("\n1.  Join the Chat\n\n");
                printf("Description: Allows you to join the chat and start sending messages.\n");
                printf("How to use it:\n");
                printf("\tSelect option 1 from the main menu.\n ");
                printf("\tAutomatically, your status will be changed to 'Online'.\n");
                printf("\tYou can start sending messages to the active channel (Global or Private).\n");
                printf("\tTo exit the chat and return to the main menu, type --exit.\n\n");
                printf("\n3. List Users\n\n");
                printf("Description: Displays a list of all online users or allows you to search for a specific user\n");
                printf("How to use it:\n");
                printf("\tYou will be asked if you want to see all users or search for a specific user:\n");
                printf("\t\tIf you choose to view all, a list of user names and statuses will be displayed.\n");
                printf("\t\tIf you choose to search for a specific user, you will need to enter the name of the desired user.\n\n");
                printf("\n4. Change Channel\n\n");
                printf("Description: Allows you to switch between the global channel or start a private chat with another user.\n");
                printf("How to use it:\n");
                printf("\t Select option 4 from the main menu.\n");
                printf("\t You will be asked if you want to switch to the global channel or set up a private channel:\n");
                printf("\t\tIf you choose the global channel, you will be informed that the channel has changed to 'Global'.\n");
                printf("\t\tIf you prefer a private channel, you will need to enter the name of the user you wish to chat with.\n");
                printf("\t\tIf the user is valid and different from you, the channel will change to a private chat with that user.\n\n");
                printf("\n5. View and Change My Status\n\n");
                printf("Description: allows you to view your current status and change it according to your preference.\n");
                printf("How to use it:\n");
                printf("\tSelect option 5 from the main menu.\n");
                printf("\tYour current status will be displayed.\n");
                printf("\tIf you wish to change it, answer yes to the question and select the new status from the options provided:\n");
                printf("\t\t 1 for 'Online'.\n");
                printf("\t\t 2 for 'Busy'.\n");
                printf("\t\t 3 for 'Inactive'.\n");
                printf("\tConfirm your selection and your status will be updated.\n\n");
                printf("\n6. Logout\n\n");
                printf("Description: Logs out of the chat and terminates the connection to the server.\n");
                printf("How to use it:\n");
                printf("\tSelect option 6 from the main menu.\n");
                printf("\tThe connection will be securely closed and the program will be terminated.\n");
                printf("-----------------------------------------------------------------------------\n");
                break;
            case 3:
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
            case 4:
                printf("Do you want to change the global channel? (y/n)\n");
                char answer2;
                scanf(" %c", &answer2);
                if (answer2 == 'y'){
                    channel = CHAT__MESSAGE_TYPE__BROADCAST;
                    printf("Changing to \033[0;35mGLOBAL\033[0m channel\n");
                } else {
                    printf("Type the username of the user you want to chat with\n");
                    char username[MAX_USERNAME_LENGTH];
                    scanf("%s", username);
                    char* user = get_all_users_action(username);
                    if (strlen(user) > 0){
                        if (strcmp(user, cli_name) == 0){
                            printf("You can't chat with yourself!\n");
                            continue;
                        }
                        printf("Changing to \033[0;34mPRIVATE\033[0m channel with %s\n", user);
                        channel = CHAT__MESSAGE_TYPE__DIRECT;
                        strcpy(current_chat, username);
                    } else {
                        printf("User not found!\n");
                    }
                }
                break;

            case 5:
                view_and_change_status();
                break;

            case 6:
                is_connected = 0;
                client_disconnect_action();
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