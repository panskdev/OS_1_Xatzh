#include "chat_manager.h"
#include <stdlib.h>
#include <stdbool.h>

#include "debug.h"

ChatManager* chat_manager_init() {
	ChatManager* manager = malloc(sizeof(ChatManager));
	manager -> active_chats = 0;
	return manager;
}

void chat_manager_destroy(ChatManager* manager) {
	for(int i = 0; i < MAX_CHATS; ++i) {
		if(manager ->chats[i] != NULL) free(manager ->chats[i]);
	}

	free(manager);
}

int first_available_chat_index(Chat ** array) {
	for(int i = 0; i < MAX_CHATS; ++i) {
		if(array[i] == NULL) return i;
	}

	return -1;	// shouldn't reach here
}

int first_available_index(int * array) {
	for(int i = 0; i < MAX_CHAT_SIZE; ++i) {
		if(array[i] == 0) return i;
	}

	return -1;	// shouldn't reach here
}

Chat* chat_init(int starter_pid, int chat_id, ChatManager* chat_manager) {
	if(chat_manager -> active_chats == MAX_CHATS) return NULL;

	Chat* chat = malloc(sizeof(Chat));

	chat -> active_procs = 1;
	chat -> chat_id = chat_id;
	chat -> pids[0] = starter_pid;
	

	int index = first_available_chat_index(chat_manager -> chats);
	chat_manager -> chats[index] = &chat;
	++(chat_manager -> active_chats);
	
	return chat;
}

int first_available_index(int * pid_array) {
	for(int i = 0; i < MAX_CHAT_SIZE; ++i) {
		if(pid_array[i] == 0) return i;
	}

	return -1;	// shouldn't reach here
}

void chat_enter(int pid, Chat* chat) {
	if(chat -> active_procs == MAX_CHAT_SIZE) return;
	int index = first_available_index(chat -> pids);
	chat -> pids[(chat -> active_procs)++] = pid;
}

int get_pid_index(int pid, Chat* chat) {
	for(int i = 0; i < MAX_CHAT_SIZE; ++i) {
		if(chat -> pids[i] == pid) return i;
	}

	return -1;
}

int get_chat_index(Chat* chat, ChatManager* manager) {
	for(int i = 0; i < MAX_CHATS; ++i) {
		if(manager -> chats[i] == chat) return i;
	}

	TEST
	return -1;
}

void chat_leave(int pid, Chat* chat, ChatManager* manager) {
	int pid_index = get_pid_index(pid, chat);
	if(pid_index == -1) return;

	chat -> pids[pid_index] = 0;

	--(chat -> active_procs);
	if(chat -> active_procs == 0) chat_destroy(chat, manager);	
}

bool chat_destroy(Chat* chat, ChatManager* manager) {
	int index = get_chat_index(chat, manager);
	if(index == -1) return false;

	free(chat);
	manager -> chats[index] = NULL;
	--(manager -> active_chats);

	if(manager -> active_chats) return true;
	return false;
	// if(manager -> active_chats) free_shm_and_stuff();
}