#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "client-node.h"
#include "chat.pb-c.h"
#include "env.h"
#include <time.h>

pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

int srv_socket_descript = 0;
CNode *root_usr = NULL, *current_usr = NULL;

void exit_service(int signal) {
    // Temporary node to free the memory after closing the connection
    CNode *to_free;
    // While there are users in the list, close the connection and free the memory
    while (root_usr) {
        printf("Connection closed for %s\n", root_usr->ip);
        // Save the node to free
        to_free = root_usr;
        // Move to the next node
        root_usr = root_usr->linked_to;
        // Free the memory of saved node
        free(to_free);
    }
    pthread_mutex_destroy(&status_mutex);
    printf("\nShutting down...\n");
    exit(EXIT_SUCCESS);
}

/*
* Status service function
* @param client_node: the client node
* @return: void
* This function will be used to check the status of the client
*/
void* status_service(void *client_node) {
    CNode *client = (CNode *) client_node;
    while(client->active) {
        if(client->status != CHAT__USER_STATUS__BUSY) {
            if((clock()-client->last_seen)/CLOCKS_PER_SEC > MAX_INACTIVE_TIME) {
                // Block the mutex while changing the status
                pthread_mutex_lock(&status_mutex);
                client->status = CHAT__USER_STATUS__BUSY;
                pthread_mutex_unlock(&status_mutex);
                printf("User %s is now busy!\n", client->name);
            }
        }
    }
    return NULL;
}

void print_users() {
    CNode *current = root_usr;
    while(current) {
        printf("User: %s\n", current->name);
        current = current->linked_to;
    }
}

/*
* Remove client service function
* @param to_remove: the client node to remove
* @return: void
* This function will be used to remove the client from the list
*/
void remove_client_service(CNode *to_remove) {
    // Block the mutex while removing the client
    pthread_mutex_lock(&client_mutex);
    if (to_remove->linked_from) {
        to_remove->linked_from->linked_to = to_remove->linked_to;
    }
    if (to_remove->linked_to) {
        to_remove->linked_to->linked_from = to_remove->linked_from;
        if(to_remove == current_usr) {
            current_usr = to_remove->linked_from;
        }
    }
    // Close the connection
    close(to_remove->data);
    // Change the status to inactive to stop the status service 
    to_remove->active = 0; 
    printf("Connection closed for %s\n", to_remove->name);
    // Free the memory
    free(to_remove); 
    // Unlock the mutex
    pthread_mutex_unlock(&client_mutex);
}

/*
* Client service function
* @param client_node: the client node
* @return: void
* This function will be used to handle the client service and actions
*/
void* client_service(void *client_node) {
    // initialize the status mutex
    pthread_mutex_init(&status_mutex, NULL);
    // Cast the client node
    CNode *client = (CNode *) client_node;
    // Create a new thread for the status service
    pthread_t thread;
    pthread_create(&thread, NULL, status_service, client_node);
    if (pthread_detach(thread) != 0) {
        printf("Status thread creation failed!\n");
        exit(EXIT_FAILURE);
    }
    
    // Create a buffer for the username and payload
    char username[MAX_USERNAME_LENGTH];
    char payload_buffer[MAX_MESSAGE_LENGTH];

    while(1){
        // Await for any incoming messages
        int raw_payload = recv(client->data, payload_buffer, MAX_MESSAGE_LENGTH, 0);
        // Check if the message is received successfully
        if (raw_payload == -1) {
            printf("Receiving message failed!\n");
            exit(EXIT_FAILURE);
        } else if (raw_payload == 0) { // Check if the client disconnected
            remove_client_service(client);
            break;
        } else {
            printf("Received message from %s: %s\n", client->name, payload_buffer);
            client->last_seen = clock();
            client->status = CHAT__USER_STATUS__ONLINE;
        }
    }
    return NULL;
}

/*
* Main function
* @param argc: number of arguments
* @param argv: arguments
* @return: 0 if successful, 1 if failed
* Based of https://www.geeksforgeeks.org/tcp-server-client-implementation-in-c/
*/
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Provide a port number!\n");
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // Save the port number
    int port = atoi(argv[1]);

    signal(SIGINT, exit_service);

    // Socket creation
    srv_socket_descript = socket(AF_INET, SOCK_STREAM, 0);
    // Check if the socket is created successfully
    if (srv_socket_descript == -1) {
        printf("Socket creation failed!\n");
        exit(EXIT_FAILURE);
    } else{
        printf("Socket created successfully!\n");
    }

    // Save the server address and client address
    struct sockaddr_in srv_address, client_address;
    int srv_addr_len = sizeof(srv_address);
    int cli_addr_len = sizeof(client_address);

    // Initialize the server and client address
    memset(&srv_address, 0, srv_addr_len);
    memset(&client_address, 0, cli_addr_len);

    // Set the server address and port
    srv_address.sin_family = AF_INET;
    srv_address.sin_addr.s_addr = INADDR_ANY;
    srv_address.sin_port = htons(port);

    // Bind the socket to the server address
    if (bind(srv_socket_descript, (struct sockaddr *) &srv_address, srv_addr_len) == -1) {
        printf("Binding failed!\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Binding successful!\n");
    }

    // Listen for incoming connections
    if (listen(srv_socket_descript, 5) == -1) {
        printf("Listening failed!\n");
        exit(EXIT_FAILURE);
    } else {
        printf("Socket is listening...\n");
    }

    // Configure the client ip and port
    getsockname(srv_socket_descript, (struct sockaddr *) &srv_address, &srv_addr_len);
    printf("Server started on %s:%d\n", inet_ntoa(srv_address.sin_addr), ntohs(srv_address.sin_port));

    // Create the root node of the tree, this will be the server
    root_usr = create_node(srv_socket_descript, inet_ntoa(srv_address.sin_addr), "Server");

    // Set the current user to the root user
    current_usr = root_usr;

    // Accept the incoming connections
    while(1){
        // Accept the incoming connection
        int cli_socket_descript = accept(srv_socket_descript, (struct sockaddr *) &client_address, (socklen_t *) &cli_addr_len);
        if (cli_socket_descript == -1) {
            printf("Accepting connection failed!\n");
            exit(EXIT_FAILURE);
        } else {
            printf("Accepted connection from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
        }

        // Create a new node for the client
        CNode *new_usr = create_node(cli_socket_descript, inet_ntoa(client_address.sin_addr), NULL);

        // Add the new node to the list
        new_usr->linked_from = current_usr;
        current_usr->linked_to = new_usr;
        current_usr = new_usr;

        // Create a new thread for the client
        pthread_mutex_init(&client_mutex, NULL);
        pthread_t thread;
        pthread_create(&thread, NULL, client_service, (void *) new_usr);
        if (pthread_detach(thread) != 0) {
            printf("Thread creation failed!\n");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}