#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
typedef struct LinkedListNode LinkedListNode;

struct LinkedListNode{
    LinkedListNode *next;
};

LinkedListNode* _insert_node_into_singlylinkedlist(LinkedListNode *head){
    if(head == NULL) {
        head = (LinkedListNode *) (malloc(sizeof(LinkedListNode)));
        head->next = NULL;
    }
    else {
        LinkedListNode *end = head;
        while (end->next != NULL) {
            end = end->next;
        }
        LinkedListNode *node = (LinkedListNode *) (malloc(sizeof(LinkedListNode)));
        node->next = NULL;
        end->next = node;
    }
    return head;
}


/*
 * Complete the function below.
 */
/*
For your reference:
LinkedListNode {
    LinkedListNode *next;
};
*/
bool readed(LinkedListNode node, LinkedListNode* readedNodes, int readedSize)
{
    int i;
    for(i = 0; i < readedSize; i++) {
        if (memcmp(&node, &readedNodes[i], sizeof(LinkedListNode))) {
            return true;
        }
    }
    return false;
}

LinkedListNode* filter(LinkedListNode* L) {
    LinkedListNode* readedNode = (LinkedListNode*) malloc(sizeof(LinkedListNode));

    readedNode[0] = *L;
    L = L->next;
    int i = 0;

    do {
        if(readed(*L, readedNode, i + 1)) {
            return L;
        }
        i++;
        readedNode = (LinkedListNode*) realloc(readedNode, (i + 1) * sizeof(LinkedListNode));
        readedNode[i] = *L;
        L = L->next;
    } while (L->next != NULL);

    return NULL;
}

int main() {
    FILE *f = fopen(getenv("OUTPUT_PATH"), "w");
    LinkedListNode* res;

    int _C_size;
    LinkedListNode* _C = NULL;
    _C_size=(rand()%100);
    int _C_i;
    for(_C_i = 0; _C_i < _C_size; _C_i++) {
        _C = _insert_node_into_singlylinkedlist(_C);
    }

    LinkedListNode *end = _C;
    while (end->next != NULL) {
        end = end->next;
    }
    end->next=_C;

    int _L_size;
    LinkedListNode* _L = NULL;
    _L_size=(rand()%100)+1;
    int _L_i;
    for(_L_i = 0; _L_i < _L_size; _L_i++) {
        _L = _insert_node_into_singlylinkedlist(_L);
    }

    end = _L;
    while (end->next != NULL) {
        end = end->next;
    }
    end->next=_C;
    res = filter(_L);

    if(res==_C)
        fprintf(f,"True\n");
    else
        fprintf(f,"False\n");

    fclose(f);
    return 0;
}
