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

Chat* chat_init(Manager* manager, int starter_pid, int chat_id) {
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
	// mutex = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, SEM_PERMS, 1);

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

	for(int i = 0; i < MAX_MSGS; ++i) {
		((chat.mailbox)[i]).msg_id = -1;
	}

	for(int i = 1; i < MAX_PARTICIPANTS; ++i) {
		(chat.participants[i]).pid = -1;
	}

	manager -> chat_inited = true;
	++(manager -> chats_active);

	int first_idx = first_available_chat_slot(manager -> chats);
	// int first_idx = (manager -> chats_active++);
	// printf("INDX IS %d\n", first_idx);
	(manager -> chats)[first_idx] = chat;

	Chat* chat_ptr = first_idx < 0 ? NULL : &((manager -> chats )[first_idx]);
	CALL_SEM(sem_post(&(manager -> manage_lock)), NULL);

	enter_chat(chat_ptr, starter_pid);
	// printf("ptr is %p\n", chat_ptr);
	return chat_ptr;
}

Chat* find_chat(Manager* manager, int chat_id) {
	CALL_SEM(sem_wait(&(manager -> manage_lock)), NULL);

	for(int i = 0; i < MAX_CHATS; ++i) {

		Chat curr = manager -> chats[i];

		if( curr.chat_id == chat_id) {
	
			CALL_SEM(sem_post(&(manager -> manage_lock)), NULL);
			return &(manager -> chats[i]);
	
		}
	}
	CALL_SEM(sem_post(&(manager -> manage_lock)), NULL);
	return NULL;	
}

void* chat_read(chat_participant_pair* pair) {
    while(1) {
        // 1. Wait for data
        CALL_SEM(sem_wait(&(pair -> chat -> full)), NULL)
        CALL_SEM(sem_wait(&(pair -> chat -> chat_lock)), NULL)

        Participant *me = pair -> participant; // Use POINTER to keep state updated
        int min_message_idx = -1;
        int lowest_found_id = -1;

        // 2. Search Buffer
        for(int i = 0; i < MAX_MSGS; ++i) {
            Message *msg = &((pair -> chat -> mailbox)[i]);
            
            // Check if valid AND if it is a NEW message (ID > my last seen)
            if(msg->msg_id != -1 && 
               msg->sender_pid != me->pid && 
               msg->msg_id > me->latest_msg_id) { // <--- Ensure latest_msg_id starts at -1

                if (min_message_idx == -1 || msg->msg_id < lowest_found_id) {
                    min_message_idx = i;
                    lowest_found_id = msg->msg_id;
                }
            }
        }

        // 3. Handle "Nothing Found"
        if(min_message_idx == -1) {
            CALL_SEM(sem_post(&(pair -> chat -> chat_lock)), NULL)
            
            // IMPORTANT: Put the 'full' token back!
            CALL_SEM(sem_post(&(pair -> chat -> full)), NULL)
            
            sleep(1); // Wait a bit before checking again
            continue;
        }

        // 4. Handle "Message Found"
        Message *target = &(pair->chat->mailbox[min_message_idx]);
        printf("%d has changed diplomatic relations with us: %s\n", target->sender_pid, target->text);
		if(strcmp(target -> text, "TERMINATE") == 0) {
			pair ->chat -> must_be_destroyed = true;
			CALL_SEM(sem_post(&(pair -> chat -> chat_lock)), NULL)
            CALL_SEM(sem_post(&(pair -> chat -> empty)), NULL) // Post EMPTY (slot freed)
			return NULL;
		}
        
        me->latest_msg_id = target->msg_id; // Remember we read this
        target->seen_num++;

        // 5. Delete or Keep
        if(target->seen_num >= pair -> chat -> participant_num) {
            target->msg_id = -1; // Delete
            CALL_SEM(sem_post(&(pair -> chat -> chat_lock)), NULL)
            CALL_SEM(sem_post(&(pair -> chat -> empty)), NULL) // Post EMPTY (slot freed)
        }
    }
    return NULL;
}

void* chat_write(chat_participant_pair* pair) {
    Participant participant = *(pair -> participant);
    // LOOP STARTS HERE
    while(1) {
		printf("Write your message...\n");
        
        // 1. Get input (Wait for user)
        if (fgets(participant.msg_buf, MSG_TEXT_SIZE, stdin) == NULL) break; 
		
        // 2. Wait for space in buffer
        CALL_SEM(sem_wait(&(pair -> chat -> empty)), NULL)
        CALL_SEM(sem_wait(&(pair -> chat -> chat_lock)), NULL)
		
		if(pair -> chat -> must_be_destroyed) {
			CALL_SEM(sem_post(&(pair -> chat -> chat_lock)), NULL)			
			return NULL;
		}
        // 3. Find slot and write
        for(int i = 0; i < MAX_MSGS; ++i) {
            Message *message = &(pair -> chat -> mailbox)[i];
            if(message -> msg_id == -1) {
                // ... (assignment logic is fine) ...
				message -> msg_id = (pair -> chat -> messages_sent)++;
				message -> seen_num = 1;
				message ->sender_pid = pair -> participant->pid;
                strcpy(message ->text, participant.msg_buf);
                break; // Break the FOR loop, not the while
            }
        }

        // 4. Release locks
        CALL_SEM(sem_post(&(pair -> chat -> chat_lock)), NULL)      
        CALL_SEM(sem_post(&(pair -> chat -> full)), NULL)
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
		// printf("i is %d\n", i);
		if((chat -> participants[i]).pid == -1) break;
	}
	
	if(i == MAX_PARTICIPANTS) {
		CALL_VOID_SEM(sem_post(&(chat -> chat_lock)));
		printf("But I thought there was space!\n");
		return;
	}

	Participant* participant = &(chat -> participants[i]);
	participant -> latest_msg_id = -1;
	participant -> pid = pid;
	pthread_t* reader = &(participant -> reader);
	pthread_t* writer = &(participant -> writer);

	chat_participant_pair pair;
	pair.chat = chat;
	pair.participant = participant;

	CALL_VOID_SEM(sem_post(&(chat -> chat_lock)));
	pthread_create(writer, NULL, (void * (*)(void*)) chat_write, &pair);
	if(pthread_create(reader, NULL, (void * (*)(void*)) chat_read, &pair)) {
		perror("Thread failed!\n");
	}

	pthread_join(*reader, NULL);
	pthread_join(*writer, NULL);
}

// void clean_everything(Manager* manager, int shmid) {

// }