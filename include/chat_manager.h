#ifndef CHAT_H
#define CHAT_H

#include <stdbool.h>

#define MAX_CHATS 10
#define MAX_CHAT_SIZE 10	// 10 procs

typedef struct {
	int chat_id;
	int active_procs;
	int pids[MAX_CHAT_SIZE];
} Chat;

typedef struct {
	int active_chats;
	Chat* chats[];
} ChatManager;

// @brief Call before initng first chat
ChatManager* chat_manager_init();

// @brief Call after closing all chats
void chat_manager_destroy(ChatManager* manager);

// @param starter_pid PID of the proc that inits chat
// @param chat_id ID which the starter_pid sets
// @param chat_manager ptr to chat manager struct
// @return ptr to the new chat
Chat* chat_init(int starter_pid, int chat_id, ChatManager* chat_manager);

// @param pid PID of the proc that enters chat
// @param chat ptr to chat struct
void chat_enter(int pid, Chat* chat);

// @param pid PID of the proc leaving chat
// @param chat ptr to chat struct
bool chat_leave(int pid, Chat* chat, ChatManager* manager);

// @param chat ptr to chat struct
void chat_destroy(Chat* chat, ChatManager* manager);
#endif