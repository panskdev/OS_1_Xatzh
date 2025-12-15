chat_compile:
	rm ./build/* || true
	cd ./build && gcc -c ../src/participant.c ../src/chat.c -I ../include/ -g -lpthread
	gcc -o ./chat ./build/participant.o ./build/chat.o -I ../include/ -g -lpthread
clean:
	rm ./build/* || true && clear
# participant_run: participant_compile
# 	./build/chat