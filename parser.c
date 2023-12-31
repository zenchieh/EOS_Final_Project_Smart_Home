# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>

# include "list_operation.h"

int place_index[12] = {1,1,1,1,2,2,2,2,3,4,4,0};
int device_index[12] = {1,2,3,4,1,2,3,4,2,1,2,5};


Node* setmode_parser(int user, int mode, int* mode_table){
    Node* head = NULL;
    
    for(int i=0;i<11;i++){
        Node* newNode = createNode();

        newNode -> task.user = user;
        newNode -> task.device = i + 1;
        if(i == 0 | i == 4 | i == 9){
            newNode -> task.temp = *(mode_table + 36*user + 12*mode + i);
        }
        else{
            newNode -> task.level = *(mode_table + 36*user + 12*mode + i);
        }

        // insert to linked list
        insertAtEnd(&head, newNode);

    }
    // printf("In parser.c - the device value of the head : %d\n",head->task.device);
    return head;

}

Node* emergency_parser(){
    Node* newNode = NULL;
    newNode = createNode();
    newNode -> task.priority = 0;
    newNode-> task.device = 12; // for doors

    return newNode;
}

Node* control_parser(int* ischange, int* change, int user, int duration){

    Node* head = NULL;
    for(int i=0;i<12;i++){
        if(ischange[i] == 1){
            Node* newNode = createNode();

            newNode -> task.user = user;
            newNode -> task.device = i + 1;
            if(duration != 0){
                newNode -> task.duration = duration;
            }

            if(i == 0 | i == 4 | i == 9){
                newNode -> task.temp = change[i];
            }
            else{
                newNode -> task.level = change[i];
            }
            insertAtEnd(&head, newNode);
        }
        else{
            continue;
        }
    }

    return head;
}

Node* reservation_parser(int* ischange, int* change, int user, int time){

    Node* head = NULL;
    for(int i=0;i<12;i++){
        if(ischange[i] == 1){
            Node* newNode = createNode();

            newNode -> task.user = user;
            newNode -> task.device = i + 1;
            newNode -> task.reservation = 1;
            newNode -> task.reservation_time = time;

            if(i == 0 | i == 4 | i == 9){
                newNode -> task.temp = change[i];
            }
            else{
                newNode -> task.level = change[i];
            }
            insertAtEnd(&head, newNode);
        }
        else{
            continue;
        }
    }

    return head;
}

Node* room_preference_parser(int* ischange, int user, int* preference, int duration){

    Node* head = NULL;

    for(int i=0;i<4;i++){
        if(ischange[i]==1){

            if(i==0){
                // set bedroom
                for(int j=0;j<4;j++){
                    Node* newNode = createNode();

                    newNode -> task.user = user;
                    newNode -> task.device = j+1;
                    if(duration != 0){
                        newNode -> task.duration = duration;
                    }

                    if(j == 0){
                        newNode -> task.temp = *(preference + user*12 + j);
                    }
                    else{
                        newNode -> task.level = *(preference + user*12 + j);
                    }
                    insertAtEnd(&head, newNode);
                }
            }
            else if(i==1){
                // set livingroom
                for(int j=0;j<4;j++){
                    Node* newNode = createNode();

                    newNode -> task.user = user;
                    newNode -> task.device = j+5;
                    if(duration != 0){
                        newNode -> task.duration = duration;
                    }
                    if(j == 0){
                        newNode -> task.temp = *(preference + user*12 + j + 4);
                    }
                    else{
                        newNode -> task.level = *(preference + user*12 + j + 4);
                    }
                    insertAtEnd(&head, newNode);
                }
            }
            else if(i==2){
                // set kitchen
                Node* newNode = createNode();
                newNode -> task.user = user;
                newNode -> task.device = 9;
                if(duration != 0){
                    newNode -> task.duration = duration;
                }

                newNode -> task.level = *(preference + user*12 + 8);

                insertAtEnd(&head, newNode);
            }
            else{
                // set bathroom
                for(int j=0;j<2;j++){
                    Node* newNode = createNode();

                    newNode -> task.user = user;
                    newNode -> task.device = j+5;
                    if(duration != 0){
                        newNode -> task.duration = duration;
                    }

                    if(j == 0){
                        newNode -> task.temp = *(preference + user*12 + j + 9);
                    }
                    else{
                        newNode -> task.level = *(preference + user*12 + j + 9);
                    }
                    insertAtEnd(&head, newNode);
                }
            }
        }
    }

    return head;
}

Node* calculate_parser(){

    Node* head = NULL;
    Node* newNode = createNode();
    newNode -> task.calculate = 1;

    insertAtEnd(&head, newNode);
    
    return head;
}