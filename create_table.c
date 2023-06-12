# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/types.h> // key_t
# include <sys/shm.h>

# define STATUS_SIZE 12*sizeof(int)
# define MODE_SIZE 30*12*sizeof(int)
# define USE_TIME_SIZE 2*12*sizeof(int)
# define PREFERENCE_SIZE 10*12*sizeof(int)
# define TEMP_SIZE 3*sizeof(int)
# define STATISTIC_SIZE 

int status_shm_id;
int *status_shm; // float because of the temperature might be float

int* create_status_table(key_t key){

    if ((status_shm_id = shmget(key, STATUS_SIZE, IPC_CREAT | 0666)) < 0)
    {
        perror("shmget");
        exit(-1);
    }
    if ((status_shm = shmat(status_shm_id, NULL, 0)) == (int *) -1)
    {
        perror("shmat");
        exit(-1);
    }
    memset(status_shm,0,STATUS_SIZE);
    return status_shm;
}

int mode_shm_id;
int *mode_shm;

int* create_mode_table(key_t key){
    if ((mode_shm_id = shmget(key, MODE_SIZE, IPC_CREAT | 0666)) < 0)
    {
        perror("shmget");
        exit(-1);
    }
    if ((mode_shm = shmat(mode_shm_id, NULL, 0)) == (int *) -1)
    {
        perror("shmat");
        exit(-1);
    }
    memset(mode_shm,0,MODE_SIZE);
    return mode_shm;
}

int using_time_shm_id;
int *using_time_shm;

int* create_using_time_table(key_t key){
    if ((using_time_shm_id = shmget(key, USE_TIME_SIZE, IPC_CREAT | 0666)) < 0)
    {
        perror("shmget");
        exit(-1);
    }
    if ((using_time_shm = shmat(using_time_shm_id, NULL, 0)) == (int *) -1)
    {
        perror("shmat");
        exit(-1);
    }
    memset(using_time_shm,0,USE_TIME_SIZE);
    return using_time_shm;
}

int preference_shm_id;
int *preference_shm;

int* create_preference_table(key_t key){
    if ((preference_shm_id = shmget(key, PREFERENCE_SIZE, IPC_CREAT | 0666)) < 0)
    {
        perror("shmget");
        exit(-1);
    }
    if ((preference_shm = shmat(preference_shm_id, NULL, 0)) == (int *) -1)
    {
        perror("shmat");
        exit(-1);
    }
    memset(preference_shm,0,PREFERENCE_SIZE);
    return preference_shm;

}

int temperature_shm_id;
int *temperature_shm;

int* create_temperature_table(key_t key){
    if ((temperature_shm_id = shmget(key, TEMP_SIZE, IPC_CREAT | 0666)) < 0)
    {
        perror("shmget");
        exit(-1);
    }
    if ((temperature_shm = shmat(temperature_shm_id, NULL, 0)) == (int *) -1)
    {
        perror("shmat");
        exit(-1);
    }
    memset(temperature_shm,0,TEMP_SIZE);
    return temperature_shm;

}