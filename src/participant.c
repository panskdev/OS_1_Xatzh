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


// #define SHM_SIZE 100

int main(int argc, char** argv) {
	if(argc != 2) {
		printf("Usage: ./chat <chat_id>");
		exit(1);
	}

	int chat_id = atoi(argv[1]);
	printf("Seeing if chat #%d exists...\n", chat_id);

	key_t key = ftok("./src/participant.c", 'F');	// 1174903626
	// printf("%d\n", key);
	int shm_id = shmget(key, sizeof(sizeof(Manager)), IPC_CREAT | 0666);
	if(shm_id == -1) {
		perror("get FAILED!\n");
		exit(1);
	}

	if(sem_unlink(SEM_CHAT));
	if(sem_unlink(SEM_EMPTY));
	if(sem_unlink(SEM_FULL));
	// if(sem_unlink(SEM_MANAGER));
	// if(sem_unlink(SEM));

	Manager* shm_ptr = (Manager*) shmat(shm_id, NULL, 0);
	if(shm_ptr == (void*)-1) {
		perror("ATTACH FAILED!\n");
		exit(1);
	}

	// printf("%d\n LOL\n", shm_ptr ->chat_inited);

	// sem_t* sem_chat = sem_open(SEM_CHAT, O_CREAT | O_EXCL, SEM_PERMS, 1);
	// sem_t* sem_msgs = sem_open(SEM_MSGS, O_CREAT | O_EXCL, SEM_PERMS, 1);

	// if(sem_chat == SEM_FAILED) {
	// 	perror("CHAT SEM FAILED!\n");
	// 	return 1;
	// }

	// if(sem_msgs == SEM_FAILED) {
	// 	perror("MSGS SEM FAILED!\n");
	// 	return 1;
	// }
	
	Chat* chat = find_chat(shm_ptr, chat_id);
	// printf(" chat points to....%p\n", chat);/
	if(chat == NULL) {
		printf("Chat not found! Creating new chat...\n");
		chat = chat_init(shm_ptr, getpid(), chat_id);
		printf("Chat %d created! with you (%d) as the participant!\n", chat -> chat_id, chat -> participants[0].pid);
	}
	else {
		printf("Entering chat...\n");
		enter_chat(chat, getpid());
		printf("Chat %d entered! with you (%d) and %d as participants!\n", chat -> chat_id, chat -> participants[0].pid, chat -> participants[1].pid);
	}
	if(shm_ptr ->chats_active == MAX_CHATS) {
		printf("Sorry! No room for you...\n");
		exit(1);	
	}

	while(1);

	// exit
	chat -> chat_id = -1;
	for(int i = 0; i < MAX_PARTICIPANTS; ++i) {
		chat -> participants[i].pid = -1;
	}

	// unlink:
	// if(sem_unlink(SEM_CHAT) < 0) {
	// 	perror("SEM_UNLINK FAILED!\n");
	// }
	shmdt(shm_ptr);
	return 0;
}