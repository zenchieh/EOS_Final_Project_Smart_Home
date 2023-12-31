#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <netdb.h>
#include <semaphore.h>

# include "authentication.h"
# include "create_table.h"
# include "translate.h"
# include "costimizer.h"
# include "parser.h"
# include "scheduler.h"
# include "dispatcher.h"
# include "additional.h"

#define MAX_BUFFER_SIZE 1024
#define PORT 8080
# define MAXUSERNUM 10


Node* task_list_head = NULL; // the head of stuct list


int serverfd, clientfd;
struct sockaddr_in server_addr, client_addr;
socklen_t client_len;
char rcvBuffer[MAX_BUFFER_SIZE];
char sendBuffer[MAX_BUFFER_SIZE];


pid_t childpid;

// parameters for shared memory
key_t dev_status_key = 1234;
extern int status_shm_id; // defined in create_table.c
int* device_status;

key_t mode_key = 5678;
extern int mode_shm_id; // defined in create_table.c
int* user_mode;

key_t start_time_key = 2345;
extern int start_time_shm_id; // defined in create_table.c
int* start_time;

key_t use_time_key = 2468;
extern int using_time_shm_id; // defined in create_table.c
int* use_time;

key_t preference_key = 3456;
extern int preference_shm_id; // defined in create_table.c
int* preference;

key_t temperature_key = 4567;
extern int temperature_shm_id; // defined in create_table.c
int* temperature;

key_t expect_using_time_key = 1579;
extern int expect_using_time_shm_id;
int* expect_using_time;

key_t expect_watt_shm_key = 6543;
extern int expect_watt_shm_id;
int* expect_watt;

// parameters for semaphores
int val_pref; // for checking semaphore value
sem_t *preference_sem;

int val_mode; // for checking semaphore value
sem_t *mode_sem;

int val_using_time; // for checking sempahore value
sem_t *using_time_sem;

int value_temperature;
sem_t *temperature_sem;

sem_t *status_sem;

sem_t *start_time_sem;

sem_t *expect_time_sem;

sem_t *watt_sem;

sem_t *expect_watt_sem;

// authentication
extern User users[MAXUSERNUM];

//message queue
extern int msg_queue_id; 
extern int msg_Q_id;
// flag for mode
int is_mode = 0;

void mode_settingmsg(int connfd, int* table);
void preference_settingmsgs(int connfd, int* table);
void expect_time_settingmsgs(int connfd, int* table);
int expect_watt_settingmsgs(int connfd);

void interrupt_handler(int signum);

int socket_server()
{
    {
        // Create a TCP socket
        serverfd = socket(AF_INET, SOCK_STREAM, 0); // PF_INET will cause a loop
        if (serverfd == -1)
        {
            perror("Error: Failed to create socket");
            exit(EXIT_FAILURE);
        }

        // Bind the socket to a local address
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(PORT);

        if (bind(serverfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            perror("Error: Failed to bind socket");
            exit(EXIT_FAILURE);
        }

        // Listen for incoming connections
        if (listen(serverfd, 5) == -1)
        {
            perror("Error: Failed to listen for connections");
            exit(EXIT_FAILURE);
        }

        printf("Server is listening on port %d...\n", PORT);
        client_len = sizeof(client_addr);
    }
    return serverfd;  
}


int main()
{
    serverfd = socket_server(); // open server

    // create shared status memory
    device_status = create_status_table(dev_status_key);
    // create user specific mode table
    user_mode = create_mode_table(mode_key);
    // create start time table
    start_time = create_start_time_table(start_time_key);
    // create use time table
    use_time = create_using_time_table(use_time_key);
    // create preference table
    preference = create_preference_table(preference_key);
    // create temperature table
    temperature = create_temperature_table(temperature_key);
    // create expect using time table
    expect_using_time = create_expect_using_time_table(expect_using_time_key);    
    // create expect watt table
    expect_watt = create_expect_watt_table(expect_watt_shm_key); 

    // Create POSIX semaphore for preference table
    preference_sem = sem_open("/SEM_PREFERENCE",O_CREAT , 0666, 1);
    if(preference_sem == SEM_FAILED){
        perror("Preference_sem init failed:");  
        return -1;  
    }
    // Create POSIX semaphore for mode table
    mode_sem = sem_open("/SEM_MODE", O_CREAT, 0666, 1);
    if(mode_sem == SEM_FAILED){
        perror("Mode_sem init failed:");  
        return -1;  
    }
    // Create POSIX semaphore for using time table
    using_time_sem = sem_open("/SEM_TIME", O_CREAT, 0666, 1);
    if(using_time_sem == SEM_FAILED){
        perror("Time_sem init failed:");  
        return -1;  
    }
    // Create POSIX semaphore for temperature table
    temperature_sem = sem_open("/SEM_TEMP", O_CREAT, 0666, 1);
    if(temperature_sem == SEM_FAILED){
        perror("Temp_sem init failed:");  
        return -1;  
    }
    status_sem = sem_open("/SEM_STATUS", O_CREAT, 0666, 1);
    if(status_sem == SEM_FAILED){
        perror("Status_sem init failed:");  
        return -1;  
    }
    expect_time_sem = sem_open("/SEM_EXPECT_TIME", O_CREAT, 0666, 1);
    if(expect_time_sem == SEM_FAILED){
        perror("Expect_time_sem init failed:");  
        return -1;  
    }
    watt_sem = sem_open("/SEM_WATT", O_CREAT, 0666, 1);
    if(watt_sem == SEM_FAILED){
        perror("Watt_sem init failed:");
        return -1;  
    }
    expect_watt_sem = sem_open("/SEM_EXPECT_WATT", O_CREAT, 0666, 1);
    if(expect_watt_sem == SEM_FAILED){
        perror("Expect_watt_sem init failed:");
        return -1;  
    }
    start_time_sem = sem_open("/SEM_START_TIME", O_CREAT, 0666, 1);
    if(start_time_sem == SEM_FAILED){
        perror("start_time_sem init failed:");  
        return -1;  
    }

    while (1)
    {

        clientfd = accept(serverfd, (struct sockaddr *)&client_addr, &client_len);
        if (clientfd == -1)
        {
            perror("Error: Failed to accept connection");
            continue;
        }

        printf("New connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        

        memset(rcvBuffer, 0, MAX_BUFFER_SIZE);
        memset(sendBuffer, 0, MAX_BUFFER_SIZE);
        // read data from client
        read(clientfd, rcvBuffer, MAX_BUFFER_SIZE);
        if (strlen(rcvBuffer) > 0){
            printf("Received message from client: %s\n", rcvBuffer);
        }

        // register
        if(strncmp(rcvBuffer,"register",8)==0){

            int msglen = sprintf(sendBuffer,"Please enter your id : ");
            if ((write(clientfd,sendBuffer,msglen+1))==-1){
                perror("Error: write()\n");
                exit(-1);
            }
            memset(rcvBuffer, 0, MAX_BUFFER_SIZE);
            read(clientfd, rcvBuffer, MAX_BUFFER_SIZE);

            signup(rcvBuffer, clientfd);
            // printUserTable(users);
            continue;
        }

        int user_index_global = 0; // who log in the system
        if(strncmp(rcvBuffer,"login",5)==0){

            int authenticate = 0; // 0 : failed, 1 : successful
            authenticate = welcome(clientfd);
            user_index_global = authenticate;
            printf("authenticate result : %d\n",authenticate);
            if(authenticate == -1){
                printf("Wrong user password\n");
                printf("Close socket to client\n");
                close(clientfd);
                continue;
            }
        }
        
        
        childpid = fork();
        if (childpid >= 0)
        {

            if (childpid == 0)
            {                
                // 子程序，處理client的command，Read data from the client
                int msglen = 0;
                int is_first_loop = 0;
                int msg_queue_id = 0;

                while (1)
                {
                    int mode_flag = 0;
                    // generate a random file name for message queue key generation
                    if(is_first_loop == 0){
                        
                        // generate message queue key
                        key_t message_queue_key = 0;


                        printf("0.1-------------------\n");
                        printf("pid : %d\n",getpid());
                        message_queue_key = ftok(".", getpid());
                        printf("0.7-------------------\n");
                        // printf("message queue key : %d\n",message_queue_key);
                        msg_queue_id = msgget(message_queue_key, 0666 | IPC_CREAT);

                        printf("%d message queue id : %d\n", getpid(), msg_queue_id);
                        msglen = sprintf(sendBuffer,"%d",msg_queue_id);
                        write(clientfd,sendBuffer,msglen+1);

                        is_first_loop++;
                    }



                    memset(rcvBuffer, 0, MAX_BUFFER_SIZE);
                    memset(sendBuffer, 0, MAX_BUFFER_SIZE);
                    // read data from client
                    read(clientfd, rcvBuffer, MAX_BUFFER_SIZE);
                    if (strlen(rcvBuffer) > 0){
                        printf("Received message from client: %s\n", rcvBuffer);
                    }
                    if(strlen(rcvBuffer) == 0){continue;}        



                    // set their own mode
                    if(strncmp(rcvBuffer,"setmode",7)==0){
                        // printf("%d wants to set the user mode.\n",clientfd);
                        
                        char username[64];
                        char tmp[64];
                        int mode = 0;
                        int user_index = 0;
                        char *token;

                        token=strtok(rcvBuffer,"|"); // setmode
                        token=strtok(NULL,"|"); // user Jonathan
                        sscanf(token," %s %s",tmp,username);
                        user_index = whichuser(username,users);
                        if(user_index == 11){
                            printf("User not found!\n");
                            break;
                        }

                        token=strtok(NULL,"|"); // afternoon
                        remove_spaces(token);
                        mode = whichmode(token);
                        printf("\nmode : %d\n",mode);

                        printf("%s start to set mode %s\n",username,token);

                        int settable[12] = {0};
                        mode_settingmsg(clientfd, settable);
                        
                        sem_wait(mode_sem);
                        // sem_getvalue(mode_sem,&val_mode);
                        // printf("Begin setmode - mode sem value=%d, pid=%d\n",val_mode, getpid());
                        
                        setmode(user_mode, user_index, mode, settable);

                        sem_post(mode_sem);
                        // sem_getvalue(mode_sem,&val_mode);
                        // printf("After setmode - mode sem value=%d, pid=%d\n",val_mode, getpid());

                        // printf("Array of user mode\n");
                        // print_int_table(user_mode, 30, 12);

                    }
                    // set to specifc mode
                    else if(strncmp(rcvBuffer,"mode",4)==0){
                        mode_flag = 1;
                        char mode[64];
                        char username[64];
                        char tmp[64];
                        int user_index = 0;
                        int mode_index = 0;
                        char *token;

                        token=strtok(rcvBuffer,"|"); // mode afternoon
                        sscanf(token,"%s %s",tmp,mode);
                        mode_index = whichmode(mode);

                        token=strtok(NULL,"|"); // user Jonathan
                        sscanf(token," %s %s",tmp,username);
                        user_index = whichuser(username,users);
                        if(user_index == 11){
                            printf("User not found!\n");
                            break;
                        }

                        printf("%s wants to set to %s mode.\n",username, mode);

                        Node* newnode;

                        sem_wait(mode_sem);
                        // sem_getvalue(mode_sem,&val_mode);
                        // printf("Begin mode - mode sem value=%d, pid=%d\n",val_mode, getpid());

                        newnode = setmode_parser(user_index, mode_index, user_mode);

                        sem_post(mode_sem);
                        // sem_getvalue(mode_sem,&val_mode);
                        // printf("After mode - mode sem value=%d, pid=%d\n",val_mode, getpid());
                        // printf("In server.c - the device value of the head : %d\n",newnode->task.device);
                        // printf("In server.c - the device value of the head->next : %d\n",newnode->next->task.device);

                        scheduler(&task_list_head,newnode);
                        displayList(task_list_head);
                        is_mode = 1;
                        dispatcher(&task_list_head,device_status,is_mode,clientfd, use_time, start_time);

                    }
                    // emergency
                    else if(strncmp(rcvBuffer,"emergency",9)==0){
                        Node* newnode;
                        newnode = emergency_parser();
                        scheduler(&task_list_head,newnode);
                        displayList(task_list_head);
                        dispatcher(&task_list_head,device_status,is_mode,clientfd, use_time, start_time);
                    }
                    // normal control command
                    else if(strncmp(rcvBuffer,"control",7)==0){

                        char username[64];
                        char tmp[64];
                        char place[64];
                        char device[64];
                        char status[64];
                        int duration = 0;
                        int user_index = 0;
                        int device_index = 0;
                        
                        char *token;
                        int ischange[12] = {0}; // 1 means user wants to change the status of this device 
                        int device_report[12] = {0}; // changes for this command

                        token=strtok(rcvBuffer,"|"); // control device

                        token=strtok(NULL,"|"); // user Jonathan
                        sscanf(token," %s %s",tmp,username);
                        user_index = whichuser(username,users);
                        printf("user index: %d\n",user_index);
                        if(user_index == 11){
                            printf("User not found!\n");
                            break;
                        }

                        token=strtok(NULL,"|"); // duration 30
                        sscanf(token," %s %d",tmp,&duration);

                        // printf("In server.c - username: %s, user_index: %d.\n",username,user_index);
                        printf("%s wants to control the devices.\n", username);
                        
                        token=strtok(NULL,"|"); // place device status
                        do{
                            memset(place,0,64);
                            memset(device,0,64);
                            memset(status,0,64);

                            sscanf(token," %s %s %s",place, device, status);
                            device_index = whichdevice(place,device);

                            ischange[device_index-1] = 1;

                            sem_wait(preference_sem);
                            // sem_getvalue(preference_sem,&val_pref);
                            // printf("Begin - preference sem value=%d, pid=%d\n",val_pref, getpid());

                            if(strncmp(status,"comfort",7)==0){
                                device_report[device_index-1] = *(preference + 12*user_index + device_index - 1);

                                sem_post(preference_sem);
                                // sem_getvalue(preference_sem,&val_pref);
                                // printf("After - preference sem value=%d, pid=%d\n",val_pref, getpid());
                            }else{
                                sem_post(preference_sem);
                                // sem_getvalue(preference_sem,&val_pref);
                                // printf("After - preference sem value=%d, pid=%d\n",val_pref, getpid());

                                device_report[device_index-1] = atoi(status);
                            }
                
                            token=strtok(NULL,"|");
                
                        }while(token != NULL);

                        // printf("ischange\n");
                        // print_int_table(ischange,1,12);
                        // printf("device report\n");
                        // print_int_table(device_report,1,12);

                        Node* newnode;
                        newnode = control_parser(ischange, device_report, user_index, duration);
                        scheduler(&task_list_head,newnode);

                        displayList(task_list_head);
                        dispatcher(&task_list_head,device_status,is_mode,clientfd, use_time, start_time);
                    }
                    // reservation
                    else if(strncmp(rcvBuffer,"reservation",11)==0){

                        char username[64];
                        char tmp[64];
                        char place[64];
                        char device[64];
                        char status[64];
                        int user_index = 0;
                        int device_index = 0;
                        int time = 0;

                        char *token;
                        int ischange[12] = {0}; // 1 means user wants to change the status of this device 
                        int device_report[12] = {0}; // changes for this command

                        token=strtok(rcvBuffer,"|"); // reservation time
                        sscanf(token,"%s %d",tmp,&time);

                        token=strtok(NULL,"|"); // user Jonathan
                        sscanf(token," %s %s",tmp,username);
                        user_index = whichuser(username,users);

                        token=strtok(NULL,"|"); // place device status
                        do{
                            memset(place,0,64);
                            memset(device,0,64);
                            memset(status,0,64);

                            sscanf(token," %s %s %s",place, device, status);
                            device_index = whichdevice(place,device);
                            // printf("In server.c - place: %s, device: %s, status: %s, device index: %d.\n",place, device, status,device_index);
                            ischange[device_index-1] = 1;

                            sem_wait(preference_sem);
                            // sem_getvalue(preference_sem,&val_pref);
                            // printf("Begin - preference sem value=%d, pid=%d\n",val_pref, getpid());

                            if(strncmp(status,"comfort",7)==0){
                                device_report[device_index-1] = *(preference + 12*user_index + device_index - 1);

                                sem_post(preference_sem);
                                // sem_getvalue(preference_sem,&val_pref);
                                // printf("After - preference sem value=%d, pid=%d\n",val_pref, getpid());
                            }else{
                                sem_post(preference_sem);
                                // sem_getvalue(preference_sem,&val_pref);
                                // printf("After - preference sem value=%d, pid=%d\n",val_pref, getpid());

                                device_report[device_index-1] = atoi(status);
                            }

                            token=strtok(NULL,"|");
                        }while(token != NULL);

                        Node* newnode;
                        newnode = reservation_parser(ischange, device_report, user_index, time);

                        scheduler(&task_list_head,newnode);

                        displayList(task_list_head);
                        dispatcher(&task_list_head,device_status,is_mode,clientfd, use_time, start_time);
                    }
                    // set preference table
                    else if(strncmp(rcvBuffer,"preference",10)==0){

                        char username[64];
                        char tmp[64];
                        int user_index = 0;
                        char *token;

                        token=strtok(rcvBuffer,"|"); // preference
                        token=strtok(NULL,"|"); // user Jonathan
                        sscanf(token," %s %s",tmp,username);
                        user_index = whichuser(username,users);
                        if(user_index == 11){
                            printf("User not found!\n");
                            break;
                        }

                        printf("%s start to set preference.\n",username);

                        int settable[12] = {0}; // temp storage of what user set
                        preference_settingmsgs(clientfd, settable);

                        sem_wait(preference_sem);
                        // sem_getvalue(preference_sem,&val_pref);
                        // printf("Begin preference setting - preference sem value=%d, pid=%d\n",val_pref, getpid());

                        setpreference(preference, user_index, settable);

                        sem_post(preference_sem);
                        // sem_getvalue(preference_sem,&val_pref);
                        // printf("After preference setting - preference sem value=%d, pid=%d\n",val_pref, getpid());

                        printf("%s end of seting preference.\n",username);
                        // print_int_table(preference, 10, 12);       
                    }
                    // set room to comfort
                    else if(strncmp(rcvBuffer,"room",4)==0){

                        char username[64];
                        char tmp[64];
                        char place[64];
                        char status[64];
                        int duration;
                        int user_index = 0;
                        int place_index = 0;
                        char *token;

                        int roomisset[4] = {0}; // 1: the room has to set to comfort
                        

                        token=strtok(rcvBuffer,"|"); // room
                        token=strtok(NULL,"|"); // user Jonathan
                        sscanf(token," %s %s",tmp,username);
                        user_index = whichuser(username,users);
                        if(user_index == 11){
                            printf("User not found!\n");
                            break;
                        }

                        token=strtok(NULL,"|"); // duration 30
                        sscanf(token," %s %d",tmp,&duration);

                        token=strtok(NULL,"|"); // bedroom comfort

                        do{

                            sscanf(token," %s %s",place,status);
                            
                            place_index = whichplace(place);
                            roomisset[place_index] = 1;

                            token=strtok(NULL,"|");
                
                        }while(token != NULL);
                        

                        Node* newnode;

                        sem_wait(preference_sem);
                        // sem_getvalue(preference_sem,&val_pref);
                        // printf("Begin room setting - preference sem value=%d, pid=%d\n",val_pref, getpid());

                        newnode = room_preference_parser(roomisset, user_index, preference, duration);

                        sem_post(preference_sem);
                        // sem_getvalue(preference_sem,&val_pref);
                        // printf("After room setting - preference sem value=%d, pid=%d\n",val_pref, getpid());

                        scheduler(&task_list_head,newnode);

                        displayList(task_list_head);
                        dispatcher(&task_list_head,device_status,is_mode,clientfd, use_time, start_time);
                    }
                    // calculate bill
                    else if(strncmp(rcvBuffer,"calculate bill",14)==0){
                        
                        Node* newnode;
                        newnode = calculate_parser();

                        scheduler(&task_list_head,newnode);
                        displayList(task_list_head);
                        dispatcher(&task_list_head,device_status,is_mode,clientfd, use_time, start_time);
                    }
                    // delete user
                    else if(strncmp(rcvBuffer,"delete",6)==0){

                        int msglen = sprintf(sendBuffer,"Please enter the id you want to delete : ");
                        if ((write(clientfd,sendBuffer,msglen+1))==-1){
                            perror("Error: write()\n");
                            exit(-1);
                        }
                        memset(rcvBuffer, 0, MAX_BUFFER_SIZE);
                        read(clientfd, rcvBuffer, MAX_BUFFER_SIZE);

                        printUserTable(users);
                        // print_int_table(preference, 10, 12);
                        sem_wait(preference_sem);
                        // sem_getvalue(preference_sem,&val_pref);
                        // printf("Begin deleteUser - preference sem value=%d, pid=%d\n",val_pref, getpid());
                        sem_wait(mode_sem);
                        // sem_getvalue(mode_sem,&val_mode);
                        // printf("Begin deleteUser - mode sem value=%d, pid=%d\n",val_mode, getpid());

                        // print_int_table(user_mode, 30, 12);
                        deleteUser(rcvBuffer, clientfd, preference, user_mode);
                        // print_int_table(user_mode, 30, 12);

                        sem_post(mode_sem);
                        // sem_getvalue(mode_sem,&val_mode);
                        // printf("After deleteUser - mode sem value=%d, pid=%d\n",val_mode, getpid());
                        sem_post(preference_sem);
                        // sem_getvalue(preference_sem,&val_pref);
                        // printf("After deleteUser - preference sem value=%d, pid=%d\n",val_pref, getpid());

                        // print_int_table(preference, 10, 12);
                        printUserTable(users);
                    }
                    // set the expect time table
                    else if(strncmp(rcvBuffer,"expect time",11)==0){
                        char username[64];
                        char tmp[64];
                        int user_index = 0;
                        char *token;

                        token=strtok(rcvBuffer,"|"); // expect time
                        token=strtok(NULL,"|"); // user Jonathan

                        sscanf(token," %s %s",tmp,username);
                        user_index = whichuser(username,users);
                        if(user_index == 11){
                            printf("User not found!\n");
                            break;
                        }

                        printf("%s start to set expect device using time.\n",username);

                        int settable[12] = {0}; // temp storage of what user set
                        expect_time_settingmsgs(clientfd, settable);

                        sem_wait(expect_time_sem);
                        setusingtime(expect_using_time, user_index, settable);
                        sem_post(expect_time_sem);

                        printf("%s end of seting expect using time.\n",username);
                        // print_int_table(expect_using_time, 1, 12);    

                    }
                    // set the expect WATT table
                    else if(strncmp(rcvBuffer,"expect watt",11)==0){
                        char username[64];
                        char tmp[64];
                        int user_index = 0;
                        char *token;
                        int expect_watt_value = 0;

                        token=strtok(rcvBuffer,"|"); // expect time
                        token=strtok(NULL,"|"); // user Jonathan

                        sscanf(token," %s %s",tmp,username);
                        user_index = whichuser(username,users);
                        printf("%s user index : %d",username,user_index);
                        if(user_index == 11){
                            printf("User not found!\n");
                            break;
                        }

                        printf("%s start to set expect watt.\n",username);

                        expect_watt_value = expect_watt_settingmsgs(clientfd);

                        sem_wait(expect_watt_sem);
                        setwatt(expect_watt, user_index, expect_watt_value);
                        sem_post(expect_watt_sem);

                        printf("%s end of seting expect watt.\n",username);
                        // print_int_table(expect_watt, 1, 1);
                    }
                    // check out temperature of three rooms
                    else if(strncmp(rcvBuffer,"check temperature",17)==0){
                                            
                        char username[64];
                        char tmp[64];
                        int user_index = 0;
                        int place_index = 0;
                        char *token;
                        int ischeck[4] = {0}; // 1: want to check the temperature of this room

                        token=strtok(rcvBuffer,"|"); // check temperature
                        token=strtok(NULL,"|"); // user Jonathan
                        sscanf(token," %s %s",tmp,username);
                        user_index = whichuser(username,users);
                        printf("%s user index : %d",username,user_index);
                        if(user_index == 11){
                            printf("User not found!\n");
                            break;
                        }

                        printf("%s want to check the temperature.\n",username);

                        token=strtok(NULL,"|"); // bedroom
                        do{
                            remove_spaces(token);
                            place_index = whichplace(token);
                            printf("in check temperature - place index : %d.\n",place_index);
                            ischeck[place_index] = 1;
                
                            token=strtok(NULL,"|");
                
                        }while(token != NULL);
                        print_int_table(ischeck, 1, 12);
                        sem_wait(temperature_sem);
                        inquire_temperature(clientfd, temperature, ischeck);
                        sem_post(temperature_sem);

                        printf("End of %s checking temperature.\n", username);
                    }
                    // check for device status
                    else if(strncmp(rcvBuffer,"check device status",19)==0){
                        char username[64];
                        char tmp[64];
                        char place[64];
                        char device[64];
                        char status[64];
                        int duration = 0;
                        int user_index = 0;
                        int device_index = 0;
                        
                        char *token;
                        int ischeck[12] = {0}; // 1 means user wants to check the status of this device 

                        token=strtok(rcvBuffer,"|"); // check device status

                        token=strtok(NULL,"|"); // user Jonathan
                        sscanf(token," %s %s",tmp,username);
                        user_index = whichuser(username,users);
                        printf("user index: %d\n",user_index);
                        if(user_index == 11){
                            printf("User not found!\n");
                            break;
                        }
                        
                        printf("%s want to check the device status.\n",username);
                        printf("-------------\n");
                        token=strtok(NULL,"|"); // bedroom light
                        printf("token : %s\n",token);
                        printf("-------------\n");
                        do{
                            memset(place,0,64);
                            memset(device,0,64);

                            sscanf(token," %s %s",place, device);
                            device_index = whichdevice(place,device);
                            printf("device index: %d\n",device_index);

                            ischeck[device_index-1] = 1;
                
                            token=strtok(NULL,"|");
                            printf("token : %s\n",token);
                
                        }while(token != NULL);

                        print_int_table(ischeck, 1, 12);
                        sem_wait(status_sem);
                        inquire_status(clientfd, ischeck, device_status);
                        sem_post(status_sem);
                    }
                    // check using time
                    else if(strncmp(rcvBuffer,"check using time",16)==0){
                        char username[64];
                        char tmp[64];
                        char place[64];
                        char device[64];
                        char status[64];
                        int duration = 0;
                        int user_index = 0;
                        int device_index = 0;
                        
                        char *token;
                        int ischeck[12] = {0}; // 1 means user wants to check the status of this device 

                        token=strtok(rcvBuffer,"|"); // check using time

                        token=strtok(NULL,"|"); // user Jonathan
                        sscanf(token," %s %s",tmp,username);
                        user_index = whichuser(username,users);
                        printf("user index: %d\n",user_index);
                        if(user_index == 11){
                            printf("User not found!\n");
                            break;
                        }
                        
                        token=strtok(NULL,"|"); // bedroom light

                        do{
                            memset(place,0,64);
                            memset(device,0,64);

                            sscanf(token," %s %s",place, device);
                            device_index = whichdevice(place,device);

                            ischeck[device_index-1] = 1;
                            printf("is check : %d\n",ischeck[device_index-1]);
                
                            token=strtok(NULL,"|");
                            printf("token : %s\n",token);
                
                        }while(token != NULL);

                        print_int_table(ischeck, 1, 12);

                        sem_wait(status_sem);
                        sem_wait(using_time_sem);
                        sem_wait(start_time_sem);
                        inquire_using_time(clientfd, ischeck, use_time, start_time, device_status);
                        sem_post(start_time_sem);
                        sem_post(using_time_sem);
                        sem_post(status_sem);
                    }

                    sem_wait(temperature_sem);
                    sem_wait(preference_sem);
                    check_temperature(clientfd, temperature, preference, user_index_global,msg_queue_id);
                    sem_post(preference_sem);
                    sem_post(temperature_sem);

                    // recommendation for device using time
                    if(mode_flag == 1) sleep(10);
                    sem_wait(status_sem);
                    sem_wait(using_time_sem);
                    sem_wait(start_time_sem);
                    sem_wait(expect_time_sem);
                    check_using_time(clientfd, device_status, use_time, start_time, expect_using_time, user_index_global, msg_queue_id);
                    sem_post(status_sem);
                    sem_post(using_time_sem);
                    sem_post(start_time_sem);
                    sem_post(expect_time_sem);

                    // recommendation for watt
                    sem_wait(watt_sem);
                    sem_wait(expect_watt_sem);
                    check_using_watt(clientfd, device_status, expect_watt, user_index_global, msg_queue_id);
                    sem_post(watt_sem);
                    sem_post(expect_watt_sem);

                    printf("Device status\n");
                    print_int_table(device_status, 1, 12);
                }
            }
            else if (childpid > 0)
            {
                // Parent process,listening the clients connections...
                signal(SIGINT,interrupt_handler);
            }
        }
        else
        {
            perror("fork error");
            exit(-1);
        }
        close(clientfd);
    }
    
    close(serverfd);
    return 0;
}

void mode_settingmsg(int connfd, int* table){

    char snd[MAX_BUFFER_SIZE] = {0},rcv[MAX_BUFFER_SIZE] = {0};
    int msglen = 0;

    memset(snd,0,MAX_BUFFER_SIZE);// set the snd char array to zero
    memset(rcv,0,MAX_BUFFER_SIZE);// set the rcv char array to zero
    
    msglen = sprintf(snd,"Start of the Bedroom setting.\nTemperature of airconditioner : ");
    write(connfd,snd,msglen+1);
    read(connfd,rcv,MAX_BUFFER_SIZE);
    table[0] = atoi(rcv);
    printf("Air conditioner temperature : %d\n",*(table));

    msglen = sprintf(snd,"Brightness of lights (0-5) : ");
    write(connfd,snd,msglen+1);
    read(connfd,rcv,MAX_BUFFER_SIZE);
    table[1] = atoi(rcv);
    printf("Lights brightness : %d\n",*(table + 1));

    msglen = sprintf(snd,"Level of fan (0-3) : ");
    write(connfd,snd,msglen+1);
    read(connfd,rcv,MAX_BUFFER_SIZE);
    table[2] = atoi(rcv);
    printf("Fan level : %d\n",*(table + 2));

    msglen = sprintf(snd,"Curtain (0: close, 1: open): ");
    write(connfd,snd,msglen+1);
    read(connfd,rcv,MAX_BUFFER_SIZE);
    table[3] = atoi(rcv);
    printf("Changed curtain level : %d\n",*(table + 3));

    // living room
    msglen = sprintf(snd,"Start of the Living room setting.\nTemperature of airconditioner : ");
    write(connfd,snd,msglen+1);
    read(connfd,rcv,MAX_BUFFER_SIZE);
    table[4] = atoi(rcv);
    printf("Air conditioner temperature : %d\n",*(table + 4));

    msglen = sprintf(snd,"Brightness of lights (0-5) : ");
    write(connfd,snd,msglen+1);
    read(connfd,rcv,MAX_BUFFER_SIZE);
    table[5] = atoi(rcv);
    printf("Lights brightness : %d\n",*(table + 5));

    msglen = sprintf(snd,"Level of fan (0-3) : ");
    write(connfd,snd,msglen+1);
    read(connfd,rcv,MAX_BUFFER_SIZE);
    table[6] = atoi(rcv);
    printf("Fan level : %d\n",*(table + 6));

    msglen = sprintf(snd,"Curtain (0: close, 1: open): ");
    write(connfd,snd,msglen+1);
    read(connfd,rcv,MAX_BUFFER_SIZE);
    table[7] = atoi(rcv);
    printf("Changed curtain level : %d\n",*(table + 7)); 

    // kitchen
    msglen = sprintf(snd,"Start of the Kitchen setting.\nBrightness of lights (0-5) : ");
    write(connfd,snd,msglen+1);
    read(connfd,rcv,MAX_BUFFER_SIZE);
    table[8] = atoi(rcv);
    printf("Lights brightness : %d\n",*(table + 8));

    // bathroom
    msglen = sprintf(snd,"Start of the Bathroom setting.\nTemperature of airconditioner : ");
    write(connfd,snd,msglen+1);
    read(connfd,rcv,MAX_BUFFER_SIZE);
    table[9] = atoi(rcv);
    printf("Air conditioner temperature : %d\n",*(table + 9));

    msglen = sprintf(snd,"Brightness of lights (0-5) : ");
    write(connfd,snd,msglen+1);
    read(connfd,rcv,MAX_BUFFER_SIZE);
    table[10] = atoi(rcv);
    printf("Lights brightness : %d\n",*(table + 10));

}

void preference_settingmsgs(int connfd, int* table){
    
    char snd[MAX_BUFFER_SIZE] = {0},rcv[MAX_BUFFER_SIZE] = {0};
    int msglen = 0;
    
    memset(rcv, 0, MAX_BUFFER_SIZE);
    memset(snd, 0, MAX_BUFFER_SIZE);
    msglen = sprintf(snd,"Start of the Bedroom setting.\nTemperature of airconditioner : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[0] = atoi(rcv);
    printf("Air conditioner temperature : %d\n",*(table));

    msglen = sprintf(snd,"Brightness of lights (0-5) : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[1] = atoi(rcv);
    printf("Lights brightness : %d\n",*(table + 1));

    msglen = sprintf(snd,"Level of fan (0-3) : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[2] = atoi(rcv);
    printf("Fan level : %d\n",*(table + 2));

    msglen = sprintf(snd,"Curtain (0: close, 1: open): ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[3] = atoi(rcv);
    printf("Changed curtain level : %d\n",*(table + 3));

    // living room
    msglen = sprintf(snd,"Start of the Living room setting.\nTemperature of airconditioner : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[4] = atoi(rcv);
    printf("Air conditioner temperature : %d\n",*(table + 4));

    msglen = sprintf(snd,"Brightness of lights (0-5) : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[5] = atoi(rcv);
    printf("Lights brightness : %d\n",*(table + 5));

    msglen = sprintf(snd,"Level of fan (0-3) : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[6] = atoi(rcv);
    printf("Fan level : %d\n",*(table + 6));

    msglen = sprintf(snd,"Curtain (0: close, 1: open): ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[7] = atoi(rcv);
    printf("Changed curtain level : %d\n",*(table + 7)); 

    // kitchen
    msglen = sprintf(snd,"Start of the Kitchen setting.\nBrightness of lights (0-5) : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[8] = atoi(rcv);
    printf("Lights brightness : %d\n",*(table + 8));

    // bathroom
    msglen = sprintf(snd,"Start of the Bathroom setting.\nTemperature of airconditioner : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[9] = atoi(rcv);
    printf("Air conditioner temperature : %d\n",*(table + 9));

    msglen = sprintf(snd,"Brightness of lights (0-5) : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[10] = atoi(rcv);
    printf("Lights brightness : %d\n",*(table + 10));   
}

void expect_time_settingmsgs(int connfd, int* table){
    char snd[MAX_BUFFER_SIZE] = {0},rcv[MAX_BUFFER_SIZE] = {0};
    int msglen = 0;
    
    memset(rcv, 0, MAX_BUFFER_SIZE);
    memset(snd, 0, MAX_BUFFER_SIZE);
    msglen = sprintf(snd,"Start of the Bedroom setting.\nExpect using time of airconditioner : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[0] = atoi(rcv);
    printf("Expect using time of air conditioner : %d\n",*(table));

    msglen = sprintf(snd,"Expect using time of lights : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[1] = atoi(rcv);
    printf("Expect using time of lights : %d\n",*(table + 1));

    msglen = sprintf(snd,"Expect using time of fan : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[2] = atoi(rcv);
    printf("Expect using time of fan : %d\n",*(table + 2));

    // msglen = sprintf(snd,"Curtain (0: close, 1: open): ");
    // write(clientfd,snd,msglen+1);
    // read(clientfd,rcv,MAX_BUFFER_SIZE);
    // table[3] = atoi(rcv);
    // printf("Changed curtain level : %d\n",*(table + 3));

    // living room
    msglen = sprintf(snd,"Start of the Living room setting.\nExpect using time of airconditioner : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[4] = atoi(rcv);
    printf("Expect using time of air conditioner : %d\n",*(table + 4));

    msglen = sprintf(snd,"Expect using time of lights : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[5] = atoi(rcv);
    printf("Expect using time of lights : %d\n",*(table + 5));

    msglen = sprintf(snd,"Expect using time of fan : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[6] = atoi(rcv);
    printf("Expect using time of fan : %d\n",*(table + 6));

    // msglen = sprintf(snd,"Curtain (0: close, 1: open): ");
    // write(clientfd,snd,msglen+1);
    // read(clientfd,rcv,MAX_BUFFER_SIZE);
    // table[7] = atoi(rcv);
    // printf("Changed curtain level : %d\n",*(table + 7)); 

    // kitchen
    msglen = sprintf(snd,"Start of the Kitchen setting.\nExpect using time of lights : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[8] = atoi(rcv);
    printf("Expect using time of lights : %d\n",*(table + 8));

    // bathroom
    msglen = sprintf(snd,"Start of the Bathroom setting.\nExpect using time of airconditioner : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[9] = atoi(rcv);
    printf("Expect using time of air conditioner : %d\n",*(table + 9));

    msglen = sprintf(snd,"Expect using time of lights : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    table[10] = atoi(rcv);
    printf("Expect using time of lights : %d\n",*(table + 10));       
}

int expect_watt_settingmsgs(int connfd){
    char snd[MAX_BUFFER_SIZE] = {0},rcv[MAX_BUFFER_SIZE] = {0};
    int msglen = 0;
    
    memset(rcv, 0, MAX_BUFFER_SIZE);
    memset(snd, 0, MAX_BUFFER_SIZE);
    msglen = sprintf(snd,"Start of the Expect watt setting.\nExpect watt : ");
    write(clientfd,snd,msglen+1);
    read(clientfd,rcv,MAX_BUFFER_SIZE);
    printf("rcv : %s\n",rcv);
    return atoi(rcv);
       
}

void interrupt_handler(int signum){


    // delete shared memory (device status)
    if (shmdt(device_status) == -1) {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(status_shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    // delete shared memory (user specific mode)
    if (shmdt(user_mode) == -1) {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(mode_shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    // delete shared memory (start time)
    if (shmdt(start_time) == -1) {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(start_time_shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }



    // delete shared memory (use time)
    if (shmdt(use_time) == -1) {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(using_time_shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    // delete shared memory (preference)
    if (shmdt(preference) == -1) {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(preference_shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    // delete shared memory (temperature)
    if (shmdt(temperature) == -1) {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(temperature_shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    // delete shared memory (expect using time)
    if (shmdt(expect_using_time) == -1) {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(expect_using_time_shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

        // delete shared memory (expect watt)
    if (shmdt(expect_watt) == -1) {
        perror("shmdt");
        exit(1);
    }

    if (shmctl(expect_watt_shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(1);
    }

    // // delete message queue
    // if(msgctl(msg_queue_id, IPC_RMID, NULL)){
    //     perror("msgctl");
    //     exit(1);
    // }
    // // delete message queue
    // if(msgctl(msg_Q_id, IPC_RMID, NULL)){
    //     perror("msgctl");
    //     exit(1);
    // }
    // delete named POSIX semphore

    sem_close(preference_sem);
    sem_unlink("/SEM_PREFERENCE");

    // delete named POSIX semphore

    sem_close(mode_sem);
    sem_unlink("/SEM_MODE");

    // delete named POSIX semaphore
    
    sem_close(using_time_sem);
    sem_unlink("/SEM_TIME");

    // delete named POSIX semaphore
    sem_close(temperature_sem);
    sem_unlink("/SEM_TEMP");

    sem_unlink("/SEM_WAIT_N_SIGNAL");

    sem_unlink("/SEM_STATUS");

    sem_close(start_time_sem);
    sem_unlink("/SEM_START_TIME");

    
    sem_close(expect_time_sem);
    sem_unlink("/SEM_EXPECT_TIME");

    sem_close(expect_watt_sem);
    sem_unlink("/SEM_EXPECT_WATT");

    sem_close(watt_sem);
    sem_unlink("/SEM_WATT");
    

    close(clientfd);
    close(serverfd);

    exit(EXIT_SUCCESS);

}