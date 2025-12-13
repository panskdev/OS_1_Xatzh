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
	manager.chat_inited = false;
	sem_t* manage_lock = sem_open(SEM_MANAGER, O_CREAT | O_EXCL, SEM_PERMS, 1);
	if(manage_lock == SEM_FAILED) {
		perror("sem open FAILED in manager creation!\n");
		exit(1);
	}
	for(int i = 0; i < MAX_CHATS; ++i) ((manager.chats)[i]).chat_id = -1;
	manager.manage_lock = *manage_lock;
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

	sem_t* chat_lock, *empty_sem, *full_sem;
	chat_lock = sem_open(SEM_CHAT, O_CREAT | O_EXCL, SEM_PERMS, 1);
	empty_sem = sem_open(SEM_EMPTY, O_CREAT | O_EXCL, SEM_PERMS, MAX_MSGS);
	full_sem = sem_open(SEM_FULL, O_CREAT | O_EXCL, SEM_PERMS, 0);

	if(chat_lock == SEM_FAILED || empty_sem == SEM_FAILED || full_sem == SEM_FAILED) {
		perror("SEM opened FAILED in chat creation\n");
		CALL_SEM(sem_post(&(manager -> manage_lock)), NULL);
		return NULL;
	}

	chat.participant_num = 0;
	// ((chat.participants)[0]).pid = starter_pid;
	chat.chat_id = chat_id;
	chat.chat_lock = *chat_lock;
	chat.empty = *empty_sem;
	chat.must_be_destroyed = false;
	chat.full = *full_sem;
	chat.messages_sent = 0;
	chat.curr_read_pos = 0;

	for(int i = 0; i < MAX_MSGS; ++i) {
		((chat.mailbox)[i]).msg_id = -1;
	}

	for(int i = 0; i < MAX_PARTICIPANTS; ++i) {
		(chat.participants[i]).pid = -1;
	}

	manager -> chat_inited = true;
	++(manager -> chats_active);

	int first_idx = first_available_chat_slot(manager -> chats);

	Chat* chat_ptr = first_idx < 0 ? NULL : &((manager -> chats )[first_idx]);
	*chat_ptr = chat;
	CALL_SEM(sem_post(&(manager -> manage_lock)), NULL);
	return chat_ptr;
}

Chat* find_chat(Manager* manager, int chat_id) {
	int x;
	sem_getvalue(&(manager -> manage_lock), &x);
	printf("sem is %d\n", x);
	CALL_SEM(sem_wait(&(manager -> manage_lock)), NULL);
	for(int i = 0; i < MAX_CHATS; i++) {

		Chat curr = (manager -> chats)[i];
		printf("Chat #%d\n", curr.chat_id);

		if( curr.chat_id == chat_id) {
	
			CALL_SEM(sem_post(&(manager -> manage_lock)), NULL);
			return &(manager -> chats[i]);
	
		}
	}
	CALL_SEM(sem_post(&(manager -> manage_lock)), NULL);
	return NULL;	
}

void * chat_write(chat_participant_pair* pair) {
    // 1. POINTERS
    Chat *chat = pair->chat;
    Participant *me = pair->participant;
    char input_buffer[MSG_TEXT_SIZE];

    while(1) {
        printf("Write: ");
        if (fgets(input_buffer, MSG_TEXT_SIZE, stdin) == NULL) continue;
        input_buffer[strcspn(input_buffer, "\n")] = 0;

        // 2. Wait & Lock
        CALL_SEM(sem_wait(&chat->empty), NULL);
        CALL_SEM(sem_wait(&chat->chat_lock), NULL);

        // 3. Write
        int idx = chat->messages_sent % MAX_MSGS;
        Message *msg = &chat->mailbox[idx];
        msg->msg_id = ++chat->messages_sent;
        msg->sender_pid = me->pid;
        msg->seen_num = 0;
        strncpy(msg->text, input_buffer, MSG_TEXT_SIZE);

        // 4. THE FIX: Loop to MAX_PARTICIPANTS, not participant_num
        for(int i = 0; i < MAX_PARTICIPANTS; i++) {
            Participant *p = &chat->participants[i];
            
            // Signal every VALID PID that is not ME
            if (p->pid != 0 && p->pid != me->pid) {
                sem_post(&p->wake_up);
            }
        }

        CALL_SEM(sem_post(&chat->chat_lock), NULL);
    }
    return NULL;
}

void * chat_read(chat_participant_pair* pair) {
    Chat *chat = pair->chat;
    Participant *me = pair->participant; 

    // Initialize local history to current state so we don't read garbage
    me->latest_msg_id = chat->messages_sent;

    while(1) {
        // 1. SLEEP on PRIVATE semaphore
        CALL_SEM(sem_wait(&me->wake_up), NULL);

        CALL_SEM(sem_wait(&chat->chat_lock), NULL);

        // 2. Catch Up Loop
        int global_max = chat->messages_sent;
        
        for (int id = me->latest_msg_id + 1; id <= global_max; id++) {
            int idx = (id - 1) % MAX_MSGS;
            Message *msg = &chat->mailbox[idx];

            // 3. Prevent Self-Echo (Do not print if I sent it)
            if (msg->sender_pid != me->pid) {
                printf("\n[PID %d] reads: %s\n", me->pid, msg->text);
                printf("Write: "); 
                fflush(stdout);
            }

            // 4. Garbage Collection
            msg->seen_num++;
            
            // Active readers = Total - 1 (The sender)
            // Ensure we don't divide by zero if alone
            int active_readers = (chat->participant_num > 1) ? (chat->participant_num - 1) : 1;
            
            if (msg->seen_num >= active_readers) {
                CALL_SEM(sem_post(&chat->empty), NULL);
            }
        }

        me->latest_msg_id = global_max;

        CALL_SEM(sem_post(&chat->chat_lock), NULL);
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

	char sem_name[128];
	sprintf(sem_name, "%s_%d_%d", SEM_WAKE, chat -> chat_id, participant -> pid);

	sem_t* wake_up = sem_open(sem_name, O_CREAT | O_EXCL, SEM_PERMS, 0);
	if(wake_up == SEM_FAILED) {
		perror("FAILED SEM WAKE UP OPEN!\n");
		return;
	}
	participant -> wake_up = *wake_up;
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
	TEST
	pthread_join(*writer, NULL);
	TEST
}

bool clean_chat(Manager* manager, Chat* chat) {
	CALL_SEM(sem_wait(&(manager -> manage_lock)), false);
	CALL_SEM(sem_wait(&(chat -> chat_lock)), false);

	chat -> chat_id = -1;
	for(int i = 0; i < chat -> participant_num; i++) {
		Participant* participant = &(chat -> participants)[i];
		participant->pid = -1;
		
		// pthread_t* writer = participant->writer;
		// pthread_t* reader = participant -> reader;
	}

	int return_val = --(manager -> chats_active) == 0 ? true : false;
	
	CALL_SEM(sem_post(&(manager -> manage_lock)), false);
	CALL_SEM(sem_post(&(chat -> chat_lock)), false);
	CALL_SEM(sem_close(&(chat -> chat_lock)), false);
	CALL_SEM(sem_close(&(chat -> full)), false);
	CALL_SEM(sem_close(&(chat -> empty)), false);
	CALL_SEM(sem_unlink(SEM_CHAT), false);
	CALL_SEM(sem_unlink(SEM_FULL), false);
	CALL_SEM(sem_unlink(SEM_EMPTY), false);


	return return_val;
}