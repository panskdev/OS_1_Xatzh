server_compile:
	rm ./build/* || true
	cd ./build && gcc -c ../src/server.c ../src/chat.c -I ../include/ -g -lpthread
	gcc -o ./build/server ./build/server.o ./build/chat.o -I ../include/ -g -lpthread

server_run: server_compile
	./build/server

participant_compile:
	rm ./build/* ./chat || true
	cd ./build && gcc -c ../src/participant.c ../src/chat.c -I ../include/ -g -lpthread
	gcc -o ./chat ./build/participant.o ./build/chat.o -I ../include/ -g -lpthread

clean:
	rm ./build/* || true && clear
# participant_run: participant_compile
# 	./build/chat