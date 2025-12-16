#pragma once

#define MAX_CHATS 10
#define MAX_PARTICIPANTS 10
#define MSG_TEXT_SIZE 256
#define MAX_MSGS 10
#define USERNAME_SIZE 25

#include <stdbool.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#define	CALL_VOID_SEM(call) {	\
	int code = call;	\
	if(code < 0) {	\
		fprintf(stderr, "Something went wrong at %d in %s (%s) : %s\n", __LINE__, __func__, __FILE__, strerror(errno));	\
		return;		\
	}	\
}

#define	CALL_SEM(call, error) {	\
	int code = call;	\
	if(code < 0) {	\
		fprintf(stderr, "Something went wrong at %d in %s (%s) : %s\n", __LINE__, __func__, __FILE__, strerror(errno));	\
		return error;		\
	}	\
}

typedef struct {
	int msg_id;
	int sender_pid;
	int seen_num;
	char text[MSG_TEXT_SIZE];
} Message;

typedef struct {
	char msg_buf[MSG_TEXT_SIZE];
	char name[USERNAME_SIZE];
	int pid;

	pthread_t writer;
	pthread_t reader;
	sem_t wake_up;
} Participant;

typedef struct {
	int chat_id;
	int participant_num;
	Participant participants[MAX_PARTICIPANTS];
	Message mailbox[MAX_MSGS];
	int messages_sent;
	sem_t chat_lock;
	sem_t empty;
} Chat;

typedef struct {
	int chats_active;
	Chat chats[MAX_CHATS];
	
	sem_t manage_lock;
} Manager;

typedef struct {
	Chat* chat;
	Participant* participant;
} chat_participant_pair;

// @return chat Manager struct
Manager manager_init();

// @brief Initializes chat with specified ID (also checks if available chat slots)
// @param manager ptr to Manager struct
// @param chat_id ID to initialize chat with
// @return ptr to Chat struct created or NULL if chat creation failed
Chat* chat_init(Manager* manager, int chat_id);

// @brief Finds chat by specified ID and returns it if found
// @param manager ptr to Manager struct
// @param chat_id ID of the chat to be found
// @return ptr to Chat struct if found or NULL otherwise
Chat* find_chat(Manager* manager, int chat_id);

// @brief Tries to insert a new participant process into given chat (does check if there are no free slots)
// @param chat ptr to Chat to enter
// @param pid Proccess ID of the process attempting to join chat
void enter_chat(Chat* chat, int pid);

// @return true if last chat (and therefore shm must be removed), false otherwise
bool clean_chat(Manager* manager, Chat* chat, int leaving_pid);