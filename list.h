#ifndef __LIST_H
#define __LIST_H

typedef void (*freeFunction)(void *);

typedef enum { FALSE, TRUE } bool;

typedef bool (*listIterator)(void *);

typedef struct _listNode {
    void *data;
    struct _listNode *next;
} listNode;

typedef struct {
    int logicalLength;
    int elementSize;
    listNode *head;
    listNode *tail;
    freeFunction freeFn;
} generic_list;

void list_new(generic_list *list, int elementSize, freeFunction freeFn);
void list_destroy(generic_list *list);

void list_prepend(generic_list *list, void *element);
void list_append(generic_list *list, void *element);
int list_size(generic_list *list);

void list_for_each(generic_list *list, listIterator iterator);
void list_head(generic_list *list, void *element, bool removeFromList);
void list_tail(generic_list *list, void *element);

#endif