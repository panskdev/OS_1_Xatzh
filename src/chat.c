#include "chat.h"

Manager* manager_init(sem_t manage_lock) {
	Manager* manager = malloc(sizeof(Manager));
	manager -> chats_active = 0;
	manager -> chat_inited = false;

	manager -> manage_lock = manage_lock;
	return manager;
}