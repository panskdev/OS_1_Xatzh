#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <errno.h>
#include "chat.h"

#include "debug.h"

int main(int argc, char** argv) {
	if(argc != 2) {
		printf("Usage: ./chat <chat_id>");
		exit(1);
	}

	key_t key = ftok("./src/participant.c", 'F');
	
	if(key == -1) {
		perror("Key token failed!\n");
		return 1;
	}
	
	// void* shm_ptr = NULL;
	int shm_id = shmget(key, sizeof(Manager), 0666);
	
	if(sem_unlink(SEM_CHAT));
	if(sem_unlink(SEM_EMPTY));
	// if(sem_unlink(SEM_FULL));
	if(sem_unlink(SEM_MANAGER));
	if(sem_unlink(SEM_WAKE));
	
	bool first_process = false;
	if(shm_id == -1) {
		shm_id = shmget(key, sizeof(Manager), IPC_CREAT | 0666);

		if(shm_id == -1) {
			perror("Get shm FAILED (1)!\n");
			return 1;
		}
		first_process = true;
	}
	
	if(shm_id == -1 && errno != EEXIST) {
		perror("Get shm FAILED (2)\n");
		return 1;
	}
	
	void* shm_ptr = (Manager*) shmat(shm_id, NULL, 0);
	if(shm_ptr == (void*)-1) {
		perror("ATTACH FAILED!\n");
		exit(1);
	}
	Manager* manager = (Manager*) shm_ptr;
	if(first_process) {
		*manager = manager_init();
	}
	
	int chat_id = atoi(argv[1]);
	printf("Seeing if chat #%d exists...\n", chat_id);
	
	if(manager ->chats_active == MAX_CHATS) {
		printf("Sorry! Active chat limit reached.\n");
		return 1;	
	}

	Chat* chat = find_chat(manager, chat_id);
	if(chat == NULL) {
		printf("Chat not found! Creating new chat...\n");
		chat = chat_init(manager, chat_id);
		// for(int i = 0; i < MAX_CHATS; i++) {
		// 	printf("CHAT #%d\n", (manager -> chats)[i].chat_id);
		// }
		enter_chat(chat, getpid());
		// printf("Chat %d created! with you (%d) as the participant!\n", chat -> chat_id, chat -> participants[0].pid);
	}
	else {
		printf("Entering chat...\n");
		enter_chat(chat, getpid());
		// printf("Chat %d entered! with you (%d) and %d as participants!\n", chat -> chat_id, chat -> participants[0].pid, chat -> participants[1].pid);
	}

	bool last_chat = clean_chat(manager, chat);
	if(last_chat) {
		shmdt(shm_ptr);
		shmctl(shm_id, IPC_RMID, NULL);
		// CALL_SEM(sem_close(&(manager -> manage_lock)), 1);
		// CALL_SEM(sem_unlink(SEM_MANAGER), 1);

		CALL_SEM(sem_destroy(&(manager -> manage_lock)), 1);
	}

	return 0;
}