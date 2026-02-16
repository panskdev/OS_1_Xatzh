## Operating Systems (K22) Assignment 1

## Usage Guide

## Disclaimer: This software works only on linux environments.

To compile, use `make compile`.
To clean working directory use `make clean` (will remove the executable, as well as any object files)
Program execution is explained below:

To execute the program, after compiling, use: `./chat <chat_number>`, in the repo's root directory. "chat_number" specifies the chat ID which the new incoming process will attempt to enter or create. In case it's the first process to run said program, it initializes the shared memory and the required semaphores appropriately.

## How the software works under the hood

If the process attempts to join a chat that already exists, then it'll check if there's room for itself. If there's no available participant slot inside aforementioned chat, then the process terminates, displaying an appropriate error message on the terminal. If there is at least one more slot for the new participating process to join, it calls the function `enter_chat`, responsible of handling this action (explained later).
If the process attempts to join a chat that never existed or no longer exists, it will attempt to create a new chat. If the maximum number of available slots for unique chat IDs has already been reached, then the process terminates and an appropriate error message is displayed on the terminal. If chat creation is successful, the new process adds itself as the sole participant thereof and the function responsible for handling joining processes inside a chat (`enter_chat`) is called.

The function `enter_chat` finds a slot for the process that attempts to join the referenced chat. If no available slot is found, then the process terminates and an error message is printed on the terminal. If a slot is found, it asks for the new process to give a unique (inside the chat) username and is persistent unless the process gives a unique valid username. Afterwards, the new participating process is presented with a message confirming their entry to the chat as well as being presented with the rest of participating processes (username + PID). The username acts as a rather user-friendly way of acknowledging the origin of messages sent in the chat. The new participating process initializes its 2 threads, `reader` and `writer`, responsible for a robust message exchange between other processes inside the same chat. We then join these 2 threads, so the process does not continue further inside `enter_chat`, unless both threads terminate. The process's threads' implementation follows:

* `reader`: The reader thread attempts to decrement the semaphore that is bound to each participant (`wake_up`). This semaphore ensures IPC implementation that features no busy waiting (as the thread is blocked, instead of constantly checking for new messages, and possibly starvating the writers). If the reader thread is ready to "wake up" it also attempts to decrement the `chat_lock` semaphore, responsible for ensuring Mutual Exclusion regarding the current chat. Afterwards, the reader gets the most recent message index inside the `mailbox` (`Message` struct array). If it reads "TERMINATE", the reader cancels the same process's writer thread and terminates, essentially terminating both threads of the process reading said message. Otherwise, the `seen_by` counter is incremented. If the message is seen by all "active" readers (all participants except for the one who composed the message) then the `chat_lock` and `empty` semaphores are posted and the reader goes back to attempting to decrement the `wake_semaphore`.

* `writer`: The writer thread first calls the `fgets` function, awaiting input from the terminal (which is the message being composed). Since `fgets` is a blocking function, it I/O blocks the writer thread until the user confirms the message. This guarantees the aforementioned non busy waiting IPC implementation. If a message is confirmed on the terminal, the thread attempts to decrement the `chat_lock` semaphore, to ensure IPC Mutual Exclusion. Afterwards it attempts to decrement the `empty` semaphore, making sure that the writer thread attempts to compose a new message only when the mailbox is empty. After appropriately updating the respective fields, it wakes up all reader threads (except the reader thread bound to the same process as said writer thread). Furthermore, it reads the message written earlier, checking if the chat is to terminate. If message written is "TERMINATE" then it also cancels the respective reader thread (and allows other processes' reader threads to handle the message appropriately.

If "TERMINATE" message is sent inside the chat, all processes enter the `clean_chat` function, which appropriately handles semaphore removal. Moreover, the last process the exit the chat invalidates the chat's slot, marking it free for future use and exits. The last exiting process, which cleans up after the last standing chat, also detaches the shared memory segment and an informative message is displayed on the terminal.

## Structs used:

* `Chat`: Defines the struct of a chat with the following fields:
	1. `chat_id`: ID number of chat.
	2. `participant_num`: The number of processes currently participating inside the chat.
	3. `participnts`: Array that stores structs of type `Participant`. Empty participant slots are represented by `Participant` structs with field value `pid = -1` (Invalid participant).
	4. `mailbox`: Array that stores structs of type `Message`, which are the chat's messages. Empty message slots are represented by `Message` structs with field value `msg_id = -1` .
	5. `messages_sent`: The total number of messages sent during the chat's lifetime.
	6. `chat_lock`: Semaphore which guarantees mutual exclusion on structs of type `Chat`.
	7. `empty`: Semaphore which represents that the (`mailbox`) is empty and can store new messages.
* `Manager`: The main struct (Shared Memory Segment's struct) with the following fields:
	1. `active_chats`: The number of currently active (valid) chats.
	2. `chats`: Array that stores structs of type `Chat`, details about each chat. A non-active chat is represented with field value `chat_id = -1` (Invalid chat).
	3. `manage_lock`: Semaphore which guarantees mutual exclusion on the the shared memory segment's struct (Chat Manager).
* `Participant`: Defines a participating process with the following fields: 
	1. `pid`: Participating process's Process ID.
	2. `writer`: Thread responsible for composing new messages inside the chat ("enables" the `reader` threads of all other participating processes).
	3. `reader`: Thread responsible for reading and displaying messages found inside the "mailbox".
	4. `wake_up`: Semaphore, given to each process, the `reader` thread of which tries decrementing before reading any new messages and is posted only by other processes' writer threads.
	5. `msg_buf`: Temporary string Buffer which stores the current process's composed message, before actually updating the mailbox and relevant fields.
 	6. `name`: A form of identification given uniquely to each process on chat entry from `stdin`. Only 2 processes not participating inside the same chat can share the same name.
* `Message`: Defines a message with the following fields:
	1. `msg_id`: Number that uniquely identifies each message inside a chat.
	2. `sender_pid`: Process ID of the respective process, the writer thread of which composed said message.
	3. `seen_num`: Number of participating processes that have seen said message. Last process to see the message, updates its field value `msg_id = -1`, which invalidates it and frees the message slot for future messages.
	4. `text`: Main text of composed message.
 	5. `sender_name`: Name of the process responsible for composing the message.
* `chat_participant_pair`: Defines a pair of pointers pointing to struct types `chat` and `participant`. This pair struct is used to correctly parse required details/fields for threads `writer` and`reader`.
