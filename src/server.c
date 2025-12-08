#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <errno.h>
#include "chat.h"

int main() {
	key_t key = ftok("participant.c", 'F');
	int shm_id = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
	char* shm_ptr = shmat(shm_id, NULL, 0);

	

	// exit
	
	shmdt(shm_id);
}