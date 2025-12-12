server_compile:
	rm ./build/server* ./*.o || true
	cd ./build && gcc -c ../src/server.c ../src/chat.c -I ../include/ -g
	gcc -o ./build/server ./build/server.o ./build/chat.o -I ../include/ -g -lpthread

server_run: server_compile
	./build/server

participant_compile:
	rm ./build/paricipant* ./*.o || true
	cd ./build && gcc -c ../src/participant.c ../src/chat.c -I ../include/ -g
	gcc -o ./build/chat ./build/participant.o ./build/chat.o -I ../include/ -g -lpthread

clean:
	rm ./build/* || true && clear
# participant_run: participant_compile
# 	./build/chat