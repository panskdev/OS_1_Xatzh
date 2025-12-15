#include "chat.h"
#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

Manager manager_init() {
	Manager manager;
	manager.chats_active = 0;

	sem_init(&(manager.manage_lock), 1, 1);

	if(&(manager.manage_lock) == SEM_FAILED) {
		perror("sem open FAILED in manager creation!\n");
		exit(1);
	}

	for(int i = 0; i < MAX_CHATS; ++i) ((manager.chats)[i]).chat_id = -1;
	return manager;
}

// @return i >= for a successful find, i = -1 for failure
int first_available_chat_slot(Chat* chats) {
	for(int i = 0; i < MAX_CHATS; ++i) {
		// printf("current id = %d at i=%d\n", (chats[i].chat_id), i);
		if((chats[i]).chat_id == -1) {
			return i;
		}
	}

	return -1;
}

Chat* chat_init(Manager* manager, int chat_id) {
	Chat chat;
	CALL_SEM(sem_wait(&(manager -> manage_lock)), NULL);

	if(manager -> chats_active == MAX_CHATS) {
		printf("Sorry! Can't create any more chats!\n");
		CALL_SEM(sem_post(&(manager -> manage_lock)), NULL);

		return NULL;
	}

	sem_init(&(chat.chat_lock), 1, 1);
	sem_init(&(chat.empty), 1, MAX_MSGS);

	chat.participant_num = 0;
	// ((chat.participants)[0]).pid = starter_pid;
	chat.chat_id = chat_id;
	chat.must_terminate = false;
	// chat.full = *full_sem;
	chat.messages_sent = 0;
	chat.curr_read_pos = 0;

	for(int i = 0; i < MAX_MSGS; ++i) {
		((chat.mailbox)[i]).msg_id = -1;
	}

	for(int i = 0; i < MAX_PARTICIPANTS; ++i) {
		(chat.participants[i]).pid = -1;
	}

	// manager -> chat_inited = true;
	++(manager -> chats_active);

	int first_idx = first_available_chat_slot(manager -> chats);

	Chat* chat_ptr = first_idx < 0 ? NULL : &((manager -> chats )[first_idx]);
	*chat_ptr = chat;
	CALL_SEM(sem_post(&(manager -> manage_lock)), NULL);
	return chat_ptr;
}

Chat* find_chat(Manager* manager, int chat_id) {
	CALL_SEM(sem_wait(&(manager -> manage_lock)), NULL);
	for(int i = 0; i < MAX_CHATS; i++) {

		Chat curr = (manager -> chats)[i];
		// printf("Chat #%d\n", curr.chat_id);

		if( curr.chat_id == chat_id) {
	
			CALL_SEM(sem_post(&(manager -> manage_lock)), NULL);
			return &(manager -> chats[i]);
	
		}
	}
	CALL_SEM(sem_post(&(manager -> manage_lock)), NULL);
	return NULL;	
}

void *chat_write(chat_participant_pair* pair) {
	Chat* chat = pair -> chat;
	Participant* participant = pair -> participant;

	while(1) {
		memset(participant -> msg_buf, 0, MSG_TEXT_SIZE);

		printf("Write a message: ");
		fflush(stdout);
		if(fgets(participant -> msg_buf, MSG_TEXT_SIZE, stdin) == NULL) continue;
		participant -> msg_buf[strcspn(participant -> msg_buf, "\n")] = 0;	// strip newline


		CALL_SEM(sem_wait(&(chat -> empty)), NULL)
		CALL_SEM(sem_wait(&(chat -> chat_lock)), NULL)

		// find the next index to put the new msg (like round robin)
		int index = chat -> messages_sent % MAX_MSGS;
		Message *message = &((chat -> mailbox)[index]);
		message -> msg_id = ++(chat -> messages_sent);
		message -> sender_pid = participant -> pid;
		message -> seen_num = 0;
		strncpy(message -> text, participant -> msg_buf, MSG_TEXT_SIZE);

		for(int i = 0; i < MAX_PARTICIPANTS; i++) {
			Participant* other_participant = &((chat -> participants)[i]);
			
			// wake up every participant that is Active/Valid AND is NOT the same as the one writing right now
			if(other_participant -> pid != -1 && other_participant -> pid != participant -> pid) {
				CALL_SEM(sem_post(&(other_participant -> wake_up)), NULL)
			}
		}
		if(!strcmp("TERMINATE", message -> text)) {
			chat -> must_terminate = true;
			CALL_SEM(sem_post(&(chat -> chat_lock)), NULL);
			pthread_cancel(participant -> reader);
			return NULL;
		}

		CALL_SEM(sem_post(&(chat -> chat_lock)), NULL)
	}
	
	return NULL;
}

void *chat_read(chat_participant_pair* pair) {
	Chat* chat = pair -> chat;
	Participant* participant = pair -> participant;

	participant -> latest_msg_id = chat -> messages_sent;	// making sure we are reading the latest news
	while(1) {
		CALL_SEM(sem_wait(&(participant -> wake_up)), NULL)	// wait till participant is woken up by a writer
		CALL_SEM(sem_wait(&(chat -> chat_lock)), NULL)

		Message* message;
		for(int i = participant -> latest_msg_id +1; i <= chat -> messages_sent; i++) {
			int index = (i - 1) % MAX_MSGS;

			message = &((chat -> mailbox)[index]);
			if(!strcmp(message -> text, "TERMINATE")) {
				pthread_cancel(pair -> participant -> writer);
				CALL_SEM(sem_destroy(&(participant -> wake_up)), NULL);
				chat -> must_terminate = true;
				CALL_SEM(sem_post(&(chat -> chat_lock)), NULL);
				CALL_SEM(sem_post(&(chat -> empty)), NULL)
				return NULL;
			}

			if(message -> sender_pid != participant -> pid) {
				printf("\n(%d) says \"%s\"\n", message -> sender_pid, message -> text);
				printf("Write a message: ");
				fflush(stdout);
			}

			++(message -> seen_num);

			// Active readers = minimum = 1 (curr participant) or everyone except the sender 
			int active_readers = chat -> participant_num > 1 ? chat -> participant_num -1 : 1;

			if(message -> seen_num >= active_readers) {
				CALL_SEM(sem_post(&(chat -> empty)), NULL)
			}
		}

		participant -> latest_msg_id = chat -> messages_sent;
		CALL_SEM(sem_post(&(chat -> chat_lock)), NULL)
	}

	return NULL;
}

void enter_chat(Chat* chat, int pid) {
	CALL_VOID_SEM(sem_wait(&(chat -> chat_lock)));
	
	if(chat -> participant_num == MAX_PARTICIPANTS) {
		CALL_VOID_SEM(sem_post(&(chat -> chat_lock)));
		printf("Sorry! Requested chat is out of free slots!\n");
		return;
	}

	int i = -1;
	for(i = 0; i < MAX_PARTICIPANTS; i++) {
		if((chat -> participants[i]).pid == -1) break;
	}
	
	if(i == MAX_PARTICIPANTS) {
		CALL_VOID_SEM(sem_post(&(chat -> chat_lock)));
		printf("But I thought there was space!\n");
		return;
	}

	int participant_index = i;

	Participant* participant = &(chat -> participants[participant_index]);
	// participant -> latest_msg_id = -1;
	participant -> pid = pid;

	sprintf(participant -> wake_up_name, "%s_%d_%d", SEM_WAKE, chat -> chat_id, participant -> pid);

	// // sem_t* wake_up = sem_open(participant -> wake_up_name, O_CREAT | O_EXCL, SEM_PERMS, 0);
	// sem_t* wake_up;
	// CALL_VOID_SEM(sem_init(wake_up, 0, 0));
	// participant -> wake_up = *wake_up;
	CALL_VOID_SEM(sem_init(&(participant -> wake_up), 1, 0));
	pthread_t* reader = &(participant -> reader);
	pthread_t* writer = &(participant -> writer);

	chat_participant_pair pair;
	pair.chat = chat;
	pair.participant = participant;

	printf("Chat %d entered with you (%d)", chat -> chat_id, chat -> participants[participant_index].pid);

	for(int i = 0; i < MAX_PARTICIPANTS; ++i) {
		if((chat -> participants)[i].pid == -1 || i == participant_index) continue;
		printf(", %d", (chat -> participants)[i].pid);
	}
	printf(" as participant(s)\n");

	CALL_VOID_SEM(sem_post(&(chat -> chat_lock)));
	pthread_create(writer, NULL, (void * (*)(void*)) chat_write, &pair);
	if(pthread_create(reader, NULL, (void * (*)(void*)) chat_read, &pair)) {
		perror("Thread failed!\n");
	}

	pthread_join(*reader, NULL);
}

bool clean_chat(Manager* manager, Chat* chat) {
	CALL_SEM(sem_wait(&(manager -> manage_lock)), false);
	CALL_SEM(sem_wait(&(chat -> chat_lock)), false);

	int return_val = false;
	if( --(chat -> participant_num) == 0) {
		chat -> chat_id = -1;

		for(int i = 0; i < chat -> participant_num; i++) {
			Participant* participant = &(chat -> participants)[i];
			participant->pid = -1;
		}

		return_val = --(manager -> chats_active) == 0 ? true : false;

		CALL_SEM(sem_destroy(&(chat -> chat_lock)), false);
		CALL_SEM(sem_destroy(&(chat -> empty)), false);
		for(int i = 0; i < MAX_PARTICIPANTS; i++) {
			(chat -> participants)[i].pid = -1;
		}
	
		for(int i = 0; i < MAX_MSGS; i++) {
			(chat -> mailbox)[i].msg_id = -1;
		}
		
		chat -> chat_id = -1;
	}
	else {
		CALL_SEM(sem_post(&(manager -> manage_lock)), false);
		CALL_SEM(sem_post(&(chat -> chat_lock)), false);
	}
	return return_val;	
}