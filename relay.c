#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/shm.h>
#include <unistd.h> // for close
#include <fcntl.h>
#include <semaphore.h>
#include <sys/time.h>

#define MAX_BUFFER_SIZE 1024
#define SEND_SIZE 27
#define DEVICE_PORT 9090
#define STATUS_PORT 9091

#define DEVICE_ID 0
#define LEVEL 1
#define TEMP 2
#define DURATION 3

#define STATUS_SHM_LENGTH 12
#define STATUS_SIZE 12 * sizeof(int)
#define MODE_SIZE 30 * 12 * sizeof(int)
# define START_TIME_SIZE 12*sizeof(int)
# define USE_TIME_SIZE 12*sizeof(int)

/*     創建 Msg Queue     */

key_t msgQkey;
int msgQid;
struct message // msgQ message format
{
    long msg_type;
    int data[4];
};
struct message msgQ;
#define MSG_Q_KEY 1111
/*     創建 Shared memory     */
key_t status_shm_key = 1234;
int *status_shm;
int status_shm_id;

key_t start_time_key = 2345;
int start_time_shm_id;
int *start_time_shm;

key_t use_time_key = 2468;
int using_time_shm_id;
int *using_time_shm;

key_t mode_shm_key = 5678;
int *mode_shm;
int mode_shm_id;

/*     創建 Semaphore     */
sem_t *status_semaphore; // 二元信號量
sem_t *useTime_semaphore;

int serverSocket, clientSocket;
int sockfd, connfd;

void interrupt_handler(int signum)
{

    // 刪除 Msg Queue
    if (msgctl(msgQid, IPC_RMID, NULL) == -1)
    {
        perror("msgctl");
        exit(1);
    }
    printf("msgQ deleted!\n");

    // 刪除 Shared memory
    if (shmdt(status_shm) == -1)
    {
        perror("shmdt status_shm");
        exit(1);
    }
    if (shmctl(status_shm_id, IPC_RMID, NULL) == -1)
    {
        perror("shmctl status_shm");
        exit(1);
    }

    if (shmdt(start_time_shm) == -1)
    {
        perror("shmdt start_time_shm");
        exit(1);
    }
    if (shmctl(start_time_shm_id, IPC_RMID, NULL) == -1)
    {
        perror("shmctl start_time_shm");
        exit(1);
    }

    if (shmdt(using_time_shm) == -1)
    {
        perror("shmdt using_time_shm");
        exit(1);
    }
    if (shmctl(using_time_shm_id, IPC_RMID, NULL) == -1)
    {
        perror("shmctl using_time_shm");
        exit(1);
    }


    close(clientSocket);
    close(serverSocket);
    close(sockfd);

    sem_close(status_semaphore);
    sem_unlink("/SEM_STATUS");
    
    sem_close(useTime_semaphore);
    sem_unlink("/SEM_TIME");
    exit(EXIT_SUCCESS);
}

void update_time_table(int *start_time_shm, int *use_time_shm, struct message msgQ)
{
    struct timeval current_time;
    if(msgQ.data[LEVEL] > 0 || msgQ.data[TEMP] > 0) // open device
    {
            gettimeofday(&current_time,NULL); // get current time
            start_time_shm[msgQ.data[DEVICE_ID] - 1] = (int)current_time.tv_sec; // update device open start time
    }
    else // for close device
    {
        if(start_time_shm[msgQ.data[DEVICE_ID] - 1] != 0) // To avoid that the device did not open yet but calculate the using time.
        {
            gettimeofday(&current_time,NULL); // get current time
            int elapsed_time = ((int)current_time.tv_sec - start_time_shm[msgQ.data[DEVICE_ID] - 1]); // current time - start time = accumulative time
            use_time_shm[msgQ.data[DEVICE_ID] - 1] += elapsed_time;
        }
    }
}


void *status_thread(void *arg)
{
    struct sockaddr_in serverAddr, clientAddr;
    char buffer[MAX_BUFFER_SIZE];
    int addrLen = sizeof(clientAddr);

    // 建立 Socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        perror("錯誤：無法建立 Socket");
        exit(1);
    }

    // 設定 Server 的 IP 和 Port
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(STATUS_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // 綁定 Socket
    if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        perror("錯誤：無法綁定 Socket");
        exit(1);
    }

    // 監聽連線請求
    if (listen(sockfd, 5) < 0)
    {
        perror("錯誤：無法監聽連線請求");
        exit(1);
    }

    printf("等待連線...\n");

    // 接受連線請求
    connfd = accept(sockfd, (struct sockaddr *)&clientAddr, &addrLen);
    if (connfd < 0)
    {
        perror("錯誤：無法接受連線請求");
        exit(1);
    }

    /*                Shared Memory                */
    {
        status_shm_id = shmget(status_shm_key, STATUS_SIZE, IPC_CREAT | 0666);
        if (status_shm_id == -1)
        {
            perror("shmget error");
            exit(1);
        }

        status_shm = (int *)shmat(status_shm_id, NULL, 0);
        if (status_shm == (int *)-1)
        {
            perror("shmat error");
            exit(1);
        }
    }
    /*                Shared Memory                */

    signal(SIGINT, interrupt_handler);
    // 接收訊息並處理
    while (1)
    {
        memset(buffer, 0, MAX_BUFFER_SIZE);

        // 接收訊息
        if (recv(connfd, buffer, MAX_BUFFER_SIZE, 0) < 0)
        {
            perror("錯誤：接收訊息失敗");
            exit(1);
        }

        if (strlen(buffer) > 0)
        {
            printf("接收到的訊息：%s", buffer);
        }

        // 將狀態寫到shared memory

        if (strcmp(buffer, "exit\n") == 0)
        {
            break;
        }
    }

    // 關閉 Socket
    close(sockfd);
}

void *command_thread(void *arg)
{
    struct sockaddr_in serverAddr, clientAddr;
    char commandBuffer[MAX_BUFFER_SIZE];

    { // 獲得 msgQ 參數
        // 產生唯一的 key
        msgQkey = MSG_Q_KEY;

        // 創建訊息佇列
        msgQid = msgget(msgQkey, IPC_CREAT | 0666);
        if (msgQid == -1)
        {
            perror("msgget");
            exit(1);
        }
    }

    { // 獲得Semaphore參數
        status_semaphore = sem_open("/SEM_STATUS", O_CREAT, 0666, 1);
        useTime_semaphore = sem_open("/SEM_TIME", O_CREAT, 0666, 1);
    }

    { // 獲得 shm 參數
        status_shm_id = shmget(status_shm_key, STATUS_SIZE, IPC_CREAT | 0666);
        if (status_shm_id == -1)
        {
            perror("shmget error");
            exit(1);
        }

        start_time_shm_id = shmget(start_time_key, START_TIME_SIZE, IPC_CREAT | 0666);
        if (start_time_shm_id == -1)
        {
            perror("shmget error");
            exit(1);
        }

        using_time_shm_id = shmget(using_time_shm_id, USE_TIME_SIZE, IPC_CREAT | 0666);
        if (using_time_shm_id == -1)
        {
            perror("shmget error");
            exit(1);
        }

        if ((status_shm = shmat(status_shm_id, NULL, 0)) == (int *) -1)
        {
            perror("shmat");
            exit(-1);
        }
        if ((start_time_shm = shmat(start_time_shm_id, NULL, 0)) == (int *) -1)
        {
            perror("shmat");
            exit(-1);
        }
        if ((using_time_shm = shmat(using_time_shm_id, NULL, 0)) == (int *) -1)
        {
            perror("shmat");
            exit(-1);
        }
    }
    { // 建立和devices的socket
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0)
        {
            perror("錯誤：無法建立 Socket");
            exit(1);
        }

        // 設定 Server 的 IP 和 Port
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(DEVICE_PORT);
        serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        // 綁定 Socket
        if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        {
            perror("錯誤：無法綁定 Socket");
            exit(1);
        }

        // 監聽連線請求
        if (listen(serverSocket, 5) < 0)
        {
            perror("錯誤：無法監聽連線請求");
            exit(1);
        }

        printf("Wait for device connection...\n");

        // 接受連線請求
        socklen_t addrLen = sizeof(clientAddr);
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrLen);
        if (clientSocket < 0)
        {
            perror("錯誤：無法接受連線請求");
            exit(1);
        }
        else
        {
            printf("Device connection successful!\n");
        }
    }
    printf("Start receive msgQ data ..\n");
    // 接收msgQ裡面的device command
    signal(SIGINT, interrupt_handler);
    while (1)
    {
        // 接收指令
        if (msgrcv(msgQid, &msgQ, sizeof(struct message) - sizeof(long), 1, 0) == -1)
        {
            perror("msgrcv error");
            exit(1);
        }
        // printf("接收到的指令：%s\n", msgQ.msg_text);

        for (int i = 0; i < 4; i++)
        {
            printf("從dispatcher透過msgQ接收到的data[%d] : %d\n", i, msgQ.data[i]);
        }

        /*          critical section          */
        sem_wait(status_semaphore);

        // 查看目前Device狀態，如果設備已開啟則不更新共享內存並設置裝置
        int sendToDevice = 0;

        if(msgQ.data[DEVICE_ID] == 1 || msgQ.data[DEVICE_ID] == 5 || msgQ.data[DEVICE_ID] == 10) // 冷氣的情況，判斷Temp
        {
            if (status_shm[msgQ.data[DEVICE_ID] - 1] != msgQ.data[TEMP]) // 確認共享內存和要設定的值是否一樣
            {
                sendToDevice = 1;
                //更新共享內存
                status_shm[msgQ.data[DEVICE_ID] - 1] = msgQ.data[TEMP];
            }
        }
        else if(msgQ.data[DEVICE_ID] == 12) // emergency
        {
            sendToDevice = 1;
            status_shm[msgQ.data[DEVICE_ID] - 1] = 1;
            for(int i=0;i<11;i++){
                status_shm[i] = 0;
            }
            status_shm[7] = 1; // 地震時窗簾打開
        }
        else
        {
            if (status_shm[msgQ.data[DEVICE_ID] - 1] != msgQ.data[LEVEL]) // 確認共享內存和要設定的值是否一樣
            {
                sendToDevice = 1;
                //更新共享內存
                status_shm[msgQ.data[DEVICE_ID] - 1] = msgQ.data[LEVEL];
            }
        }

        if(sendToDevice){
            // 讀取共享內存，製作新的指令，將處理過後的指令內容到socket buffer EX: 23 0 0 2 1 0 3 1 0 ...
            
            sem_wait(useTime_semaphore);
            update_time_table(start_time_shm,using_time_shm,msgQ); // update start time and use time.
            sem_post(useTime_semaphore);

            memset(commandBuffer,0,strlen(commandBuffer));
            char insert_str[3];

            for(int i=0;i<STATUS_SHM_LENGTH;i++)
            {
                if(i == 0 || i == 4 || i == 9){ // aircon特殊情況 為0(關閉)時補0 EX:0->00
                    if(status_shm[i] == 0){
                        strcpy(insert_str, "00");
                    }
                    else{
                        sprintf(insert_str, "%d", status_shm[i]);
                    }
                }
                else{
                    sprintf(insert_str, "%d", status_shm[i]);
                }
                //sprintf(insert_str, "%d", status_shm[i]);
                strcat(commandBuffer,insert_str);
                strcat(commandBuffer," ");
                memset(insert_str,0,strlen(insert_str));
            }
            printf("In Relay.c , Relay -> Device commandBuffer = %s.\n",commandBuffer);
            // 透過socket轉送給device
            sleep(1);
            if (send(clientSocket, commandBuffer, SEND_SIZE, 0) < 0)
            {
                perror("錯誤：發送訊息失敗");
                exit(1);
            }
        }

        // 查看共享內存溫度訊息，若溫度過高將冷氣打開到適合的溫度，再次發送一個指令
        // send()

        sem_post(status_semaphore);
        /*          critical section          */
    }
    exit(EXIT_SUCCESS);
}

int main()
{

    pthread_t threads[2];
    // 創建thread
    pthread_create(&threads[0], NULL, command_thread, NULL);
    pthread_create(&threads[1], NULL, status_thread, NULL);

    // 等待線程完成
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    return 0;
}