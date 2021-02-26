TARGET=client

client:client.c commands.o
	gcc client.c commands.o -o client -pthread

commands.o: ../commands/commands.c ../commands/commands.h quicksend.o
	gcc -r ../commands/commands.c ../commands/commands.h -pthread -o commands.o

quicksend.o: ../quicksend/quicksend.c ../quicksend/quicksend.h
	gcc -r ../quicksend/quicksend.c ../quicksend/quicksend.h -o quicksend.o
quicksend.o
