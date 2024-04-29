#ifndef CNODE
#define CNODE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {ONLINE, BUSY, OFFLINE} Status;

typedef struct node {
    int data;
    struct node *linked_to;
    struct node *linked_from;
    char name[20];
    Status status;
    char ip[16];
    clock_t last_seen;
} CNode;

CNode *create_node(int socket, char *ip){
    CNode *node = (CNode *) malloc(sizeof(CNode));
    node->data = socket;
    node->linked_to = NULL;
    node->linked_from = NULL;
    node->status = ONLINE;
    strncpy(node->ip, ip, 16);
    strncpy(node->name, "Server", 6);
    node->last_seen = clock();
    return node;
}

#endif