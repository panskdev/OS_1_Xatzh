#pragma once

#define MAX_CHATS 10
#define MAX_PARTICIPANTS 10
#define CHAT_SIZE sizeof(Chat)
#define MSG_TEXT_SIZE 256
#define INVALID_CHAT_ID -1
#define MAX_MSGS 10
#define MANAGE_SIZE sizeof(Manager)

#define SEM_MANAGER "manager_sem"
#define SEM_CHAT "chat_sem"
#define SEM_EMPTY "empty_sem"
#define SEM_FULL "full_sem"
#define SEM_WAKE "wake_sem"

#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

#include <stdbool.h>
#include <semaphore.h>
#include <pthread.h>

typedef struct {
	int msg_id;
	int chat_id;
	int sender_pid;
	int seen_num;
	char text[MSG_TEXT_SIZE];
} Message;
typedef struct {
	char msg_buf[MSG_TEXT_SIZE];
	int latest_msg_id;
	int pid;

	pthread_t writer;
	pthread_t reader;
	sem_t wake_up;
} Participant;

typedef struct {
	int chat_id;
	int participant_num;
	Participant participants[MAX_PARTICIPANTS];
	bool must_be_destroyed;
	int max_msg_unread;	// greatest id inside the chat's "mailbox", so reader threads don't keep scanning the entire array but this value if they've read the latest "news"
	Message mailbox[MAX_MSGS];
	int messages_sent;
	sem_t chat_lock;
	int curr_read_pos;

	// sems for the bounded buffer
	sem_t empty;
	sem_t full;
} Chat;

typedef struct {
	int chats_active;
	bool chat_inited;	// to destroy manager only and only if at least one chat ever started
	Chat chats[MAX_CHATS];
	
	sem_t manage_lock;
} Manager;

typedef struct {
	Chat* chat;
	Participant* participant;
} chat_participant_pair;

Manager manager_init();

Chat* chat_init(Manager* manager, int chat_id);

Chat* find_chat(Manager* manager, int chat_id);

void enter_chat(Chat* chat, int pid);

// @return true if last chat (and therefore shm must be removed), false otherwise
bool clean_chat(Manager* manager, Chat* chat);