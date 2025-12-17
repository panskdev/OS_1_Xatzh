#include "chat.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

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

	if(manager ->chats_active == MAX_CHATS) {
		printf("Sorry! Active chat limit reached.\n");
		return NULL;
	}

	sem_init(&(chat.chat_lock), 1, 1);
	sem_init(&(chat.empty), 1, 1);

	chat.participant_num = 0;
	chat.chat_id = chat_id;
	chat.messages_sent = 0;

	for(int i = 0; i < MAX_MSGS; ++i) {
		((chat.mailbox)[i]).msg_id = -1;
	}

	for(int i = 0; i < MAX_PARTICIPANTS; ++i) {
		(chat.participants[i]).pid = -1;
	}

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

		CALL_SEM(sem_wait(&(chat -> chat_lock)), NULL)

		if(chat -> participant_num > 1) {
			CALL_SEM(sem_wait(&(chat -> empty)), NULL)	// making sure one other process exists to actually post the semaphore with reader thread ...
		}

		// find the next index to put the new msg (like round robin)
		int index = chat -> messages_sent % MAX_MSGS;
		Message *message = &((chat -> mailbox)[index]);
		message -> msg_id = ++(chat -> messages_sent);
		message -> sender_pid = participant -> pid;
		message -> seen_num = 0;
		strncpy(message -> sender_name, participant -> name, USERNAME_SIZE);
		strncpy(message -> text, participant -> msg_buf, MSG_TEXT_SIZE);

		for(int i = 0; i < MAX_PARTICIPANTS; i++) {
			Participant* other_participant = &((chat -> participants)[i]);
			
			// wake up every participant that is Active/Valid AND is NOT the same as the one writing right now
			if(other_participant -> pid != -1 && other_participant -> pid != participant -> pid) {
				CALL_SEM(sem_post(&(other_participant -> wake_up)), NULL)
			}
		}

		if(!strcmp("TERMINATE", message -> text)) {
	
			pthread_cancel(participant -> reader);
			CALL_SEM(sem_post(&(chat -> chat_lock)), NULL);
	
			return NULL;
		}

		CALL_SEM(sem_post(&(chat -> chat_lock)), NULL)
	}
	
	return NULL;
}

void *chat_read(chat_participant_pair* pair) {
	Chat* chat = pair -> chat;
	Participant* participant = pair -> participant;

	while(1) {
		CALL_SEM(sem_wait(&(participant -> wake_up)), NULL)	// wait till participant is woken up by a writer
		CALL_SEM(sem_wait(&(chat -> chat_lock)), NULL)

		int index = (chat -> messages_sent - 1) % MAX_MSGS;
		Message* message = &((chat -> mailbox)[index]);
		if(!strcmp(message -> text, "TERMINATE")) {
			pthread_cancel(pair -> participant -> writer);
			CALL_SEM(sem_post(&(chat -> chat_lock)), NULL);
			return NULL;
		}
		if(message -> sender_pid != participant -> pid) {
			printf("\n(%s-%d): \"%s\"\n", message -> sender_name, message -> sender_pid, message -> text);
			printf("Write a message: ");
			fflush(stdout);
		}
		++(message -> seen_num);
		
		// (Potential) Active readers = minimum = 1 (curr participant) or everyone except the sender 
		int active_readers = chat -> participant_num > 1 ? chat -> participant_num -1 : 1;

		if(message -> seen_num >= active_readers) {
			CALL_SEM(sem_post(&(chat -> empty)), NULL)
		}

		// participant -> latest_msg_id = chat -> messages_sent;	// making sure we are reading the latest news
		CALL_SEM(sem_post(&(chat -> chat_lock)), NULL)
	}

	return NULL;
}

void enter_chat(Chat* chat, int pid) {
	CALL_VOID_SEM(sem_wait(&(chat -> chat_lock)));
	
	if(chat -> participant_num == MAX_PARTICIPANTS) {
		CALL_VOID_SEM(sem_post(&(chat -> chat_lock)));
		printf("Sorry! Requested chat is out of free slots!\n");
		exit(1);
	}

	
	int participant_index = (chat -> participant_num);
	Participant* participant = &(chat -> participants[participant_index]);
	participant -> pid = pid;
	while(1) {
		printf("Enter your username (unique): ");
		fflush(stdin);
		
		CALL_VOID_SEM(sem_post(&(chat -> chat_lock)));
		if(fgets(participant -> name, USERNAME_SIZE, stdin) == NULL) {
			continue;	
		}
		CALL_VOID_SEM(sem_wait(&(chat -> chat_lock)));

		participant -> name[strcspn(participant -> name, "\n")] = 0;	// strip newline
		if(strlen(participant -> name) == 0) {
			continue;
		} 
		
		bool username_exists = false;
		for(int i = 0; i < chat -> participant_num; i++) {
			if(!strcmp((chat -> participants[i]).name, participant -> name) && (chat -> participants[i]).pid != -1 && (chat -> participants[i]).pid != getpid()) {
				username_exists = true;
				break;
			}
		}
		if(!username_exists) break;
	}

	++(chat -> participant_num);
	CALL_VOID_SEM(sem_init(&(participant -> wake_up), 1, 0));
	pthread_t* reader = &(participant -> reader);
	pthread_t* writer = &(participant -> writer);

	chat_participant_pair pair;
	pair.chat = chat;
	pair.participant = participant;

	printf("Chat %d entered with you (%s-%d)", chat -> chat_id, participant -> name, participant -> pid);

	for(int i = 0; i < chat -> participant_num; ++i) {
		if((chat -> participants)[i].pid == -1 || i == participant_index) continue;
		printf(", %s-%d", (chat -> participants)[i].name, (chat -> participants)[i].pid);
	}
	printf(" as participant(s)\n");

	CALL_VOID_SEM(sem_post(&(chat -> chat_lock)));
	if(
		pthread_create(reader, NULL, (void * (*)(void*)) chat_read, &pair) ||
		pthread_create(writer, NULL, (void * (*)(void*)) chat_write, &pair)
	) {
		perror("Thread creation failed!\n");
	}

	pthread_join(*reader, NULL);
}

bool clean_chat(Manager* manager, Chat* chat, int leaving_pid) {
	CALL_SEM(sem_wait(&(manager -> manage_lock)), false);
	CALL_SEM(sem_wait(&(chat -> chat_lock)), false);

	printf("Leaving chat #%d...\n", chat -> chat_id);
	int last_chat = false;

	int old_id = chat -> chat_id;
	for(int i = 0; i < chat -> participant_num; i++) {
		if(leaving_pid == (chat -> participants)[i].pid) {
			CALL_SEM(sem_destroy(&((chat -> participants)[i].wake_up)), false);
			}
		}
	if( --(chat -> participant_num) == 0) {
		printf("I'm the last one to go... I have to clean up\n");

		chat -> chat_id = -1;

		for(int i = 0; i < chat -> participant_num; i++) {
			Participant* participant = &(chat -> participants)[i];
			participant->pid = -1;
		}

		last_chat = --(manager -> chats_active) == 0 ? true : false;

		CALL_SEM(sem_destroy(&(chat -> chat_lock)), false);
		CALL_SEM(sem_destroy(&(chat -> empty)), false);
		for(int i = 0; i < chat -> participant_num; i++) {
			(chat -> participants)[i].pid = -1;
		}
	
		for(int i = 0; i < MAX_MSGS; i++) {
			(chat -> mailbox)[i].msg_id = -1;
		}
		
		chat -> chat_id = -1;
	}
	CALL_SEM(sem_post(&(manager -> manage_lock)), false);
	CALL_SEM(sem_post(&(chat -> chat_lock)), false);

	if(last_chat) {
		printf("Chat #%d was the last one to go\n", old_id);
	}
	return last_chat;
}