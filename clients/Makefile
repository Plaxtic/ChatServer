TARGET=client

client:client.c commands.o
	gcc client.c commands.o -o client -pthread

commands.o: ../commands/commands.c ../commands/commands.h
	gcc -r ../commands/commands.c ../commands/commands.h -o commands.o


