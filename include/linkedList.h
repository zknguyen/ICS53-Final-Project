#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Declare structures
struct user_data;
struct auction_data;
struct job_data;

/*
 * Structure for each node of the linkedList
 *
 * value - a pointer to the data of the node. 
 * next - a pointer to the next node in the list. 
 */
typedef struct node {
    void* value;
    struct node* next;
} node_t;

/*
 * Structure for user information (Users) 
 * username - user's username
 * password - user's password
 * fd - file descriptor
 * won_auctions - linked list containing auctions user has won
 */
typedef struct user_data {
    char* username;
    char* password;
    int fd;
    node_t* won_auctions;
} user_data;

/* Structure to hold job information
 */
typedef struct job_data {
    uint8_t msg_type;
    char* buffer;
    char* sender;
} job_data;

/*
 * Structure for the base linkedList
 * 
 * head - a pointer to the first node in the list. NULL if length is 0.
 * length - the current length of the linkedList. Must be initialized to 0.
 * comparator - function pointer to linkedList comparator. Must be initialized!
 */
typedef struct list {
    node_t* head;
    int length;
} List_t;

/*
 * Structure for auction information (Auctions)
 * auctionid - auction's id
 * creator - username of user who created auction
 * item - item being auctioned
 * ticks - number of ticks remaining
 * highest_bid - current highest bid in auction
 * highest_bidder - username of highest bidder
 * watchers - linked list containing all users watching the auction
 */
typedef struct auction_data {
    int auctionid;
    char* creator;
    char* item;
    int ticks;
    int highest_bid;
    char* highest_bidder;
    List_t* watchers;
} auction_data;

/* 
 * Each of these functions inserts the reference to the data (valref)
 * into the linkedList list at the specified position
 *
 * @param list pointer to the linkedList struct
 * @param valref pointer to the data to insert into the linkedList
 */
void insertRear(List_t* list, void* valref);
void insertFront(List_t* list, void* valref);

/*
 * Each of these functions removes a single linkedList node from
 * the LinkedList at the specfied function position.
 * @param list pointer to the linkedList struct
 * @return a pointer to the removed list node
 */ 
void* removeFront(List_t* list);
void* removeRear(List_t* list);
void* removeByIndex(List_t* list, int n);

/* 
 * Free all nodes from the linkedList
 *
 * @param list pointer to the linkedList struct
 */
void deleteList(List_t* list);

/*
 * Searches linked list for user with given username
 */
user_data* searchUsers(List_t* list, char* username);

/*
 * Validates login info (username and password) 
 */
user_data* validateLogin(List_t* list, char* username, char* password);

/*
 * Traverse the list printing each node in the current order.
 * @param list pointer to the linkedList strut
 * @param mode STR_MODE to print node.value as a string,
 * INT_MODE to print node.value as an int
 */
void printList(List_t* list, char mode);

#endif
