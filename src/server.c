#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <errno.h>
#include "chat.h"
#include <sys/stat.h>
#include <fcntl.h>

#include "debug.h"

#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

int main() {
	void* shm_ptr=NULL;
	key_t key = ftok("./src/participant.c", 'F');
	if(key == -1) {
		perror("key failed!\n");
		exit(1);
	}

	if(sem_unlink(SEM_CHAT));
	if(sem_unlink(SEM_MANAGER));
	// if(sem_unlink(SEM_MSGS));

	int shm_id = shmget(key, sizeof(Manager), IPC_CREAT | 0666);

	if(shm_id == -1) {
		perror("GET failed!\n");
		exit(1);
	}
	shm_ptr = shmat(shm_id, NULL, 0);

	if(shm_ptr == (void*)-1) {
		perror("Attach failed server!\n");
		exit(1);
	}

	Manager* manager = (Manager*) shm_ptr;
	*manager = manager_init();

	shm_ptr = &manager;
	// printf("%d\n", manager -> chats_active);
	// exit
	while(!manager ->chat_inited || manager ->chats_active) {
		int should_stop = 0;
		printf("Should stop?\n");
		scanf("%d", &should_stop);

		if(should_stop) break;
	}

	if(sem_close(&(manager -> manage_lock)) < 0);

	unlink:
	if(sem_unlink(SEM_MANAGER) < 0);
	// free(manager);
	shmdt(shm_ptr);
	shmctl(shm_id, IPC_RMID, NULL);
}