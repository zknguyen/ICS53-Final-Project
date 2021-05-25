// DO NOT MODIFY THIS FILE
// Any additions should be placed in helpers2.h

#ifndef LINKEDLIST_H
#define LINKEDLIST_H
typedef struct node {
    void* data;
    struct node* next;
} node_t;

typedef struct list {
    node_t* head;
    int (*comparator)(void*, void*);
    void (*printer)(void*);
    void (*deleter)(void*);
    int length;
} list_t;

// Functions precompiled in library
list_t* GenerateDefaultMorseMappings(void);
void DestroyDefaultMorseMappings(list_t* list);

// Functions already implemented
list_t* CreateList(int (*compare)(void*, void*), void (*print)(void*),
                   void (*delete)(void*));
void PrintLinkedList(list_t* list);
void InsertAtHead(list_t* list, void* val_ref);
void InsertAtTail(list_t* list, void* val_ref);
void InsertInOrder(list_t* list, void* val_ref);
#endif
