#pragma once

#define MAX_CHATS 10
#define MAX_PARTICIPANTS 10
#define CHAT_SIZE sizeof(Chat)
#define MSG_TEXT_SIZE 256
// #define MAX_MSGS 10
#define MANAGE_SIZE sizeof(Manager)
#define SHM_SIZE MAX_CHATS * CHAT_SIZE + MANAGE_SIZE

#include <stdbool.h>
#include <semaphore.h>

typedef struct {
	int msg_id;
	int chat_id;
	int seen_num;
	char text[MSG_TEXT_SIZE];
} Message;

typedef struct {
	int chat_id;
	int participant_num;
	int write_index;
	int participant_pids[MAX_PARTICIPANTS];
	Message messages[2][2];
	sem_t chat_lock;
} Chat;

typedef struct {
	int chats_active;
	bool chat_inited;	// to destroy manager only and only if at least one chat ever started
	Chat chats[MAX_CHATS];
	
	sem_t manage_lock;
} Manager;


Manager* manager_init(sem_t manage_lock);