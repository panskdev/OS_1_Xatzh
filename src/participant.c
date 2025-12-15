#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "chat.h"

int main(int argc, char** argv) {
	if(argc != 2) {
		printf("Usage: ./chat <chat_id>\n");
		exit(1);
	}

	key_t key = ftok("./src/participant.c", 'F');
	
	if(key == -1) {
		perror("Key token failed!\n");
		return 1;
	}
	
	int shm_id = shmget(key, sizeof(Manager), 0666);
	
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

	Chat* chat = find_chat(manager, chat_id);
	if(chat == NULL) {
		printf("Chat not found! Creating new chat...\n");
		chat = chat_init(manager, chat_id);
		if(chat == NULL) return 1;
		enter_chat(chat, getpid());
	}
	else {
		printf("Entering chat...\n");
		enter_chat(chat, getpid());
	}

	bool last_chat = clean_chat(manager, chat, getpid());
	if(last_chat) {
		shmdt(shm_ptr);
		shmctl(shm_id, IPC_RMID, NULL);
		CALL_SEM(sem_destroy(&(manager -> manage_lock)), 1);
	}

	return 0;
}