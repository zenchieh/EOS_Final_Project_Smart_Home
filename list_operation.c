# include <stdio.h>
# include <stdlib.h>

typedef struct Task {
    int priority;
    int user; // 1-10
    int reservation; // 0 or 1
    int place; // 0-3
    int device; // 0-5
    int level; // 0-3 for fan
    int temp; // for air conditioner
    int duration;
    int calculate; // 0 or 1
} Task;

// struct of the linked list node
typedef struct Node {
    Task task;
    struct Node* next;
} Node;



Node* createNode() {
    Node* newNode = (Node*)malloc(sizeof(Node));

    newNode->task.priority = 1;
    newNode->task.user = 0;
    newNode->task.reservation = 0;
    newNode->task.place = 0;
    newNode->task.device = 0;
    newNode->task.level = 0;
    newNode->task.temp = 0;
    newNode->task.duration = 0;
    newNode->task.calculate = 0;

    newNode->next = NULL;
    return newNode;
}

void insertAtBeginning(Node** head, Node* newNode) {

    newNode->next = *head;
    *head = newNode;
}

void insertAtEnd(Node** head, Node* newNode) {

    if (*head == NULL) {
        *head = newNode;
    } else {
        Node* temp = *head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = newNode;
    }
}

// void displayList(Node* head) {
//     if (head == NULL) {
//         printf("List is empty.\n");
//     } else {
//         Node* temp = head;
//         printf("List: ");
//         while (temp != NULL) {
//             printf("%d ", temp->task.device);
//             temp = temp->next;
//         }
//         printf("\n");
//     }
// }