TARGET=chat_server

chat_server:chat_server.c commands.o
	gcc chat_server.c commands.o -o chat_server

commands.o:commands/commands.c commands/commands.h
	gcc -r commands/commands.c commands/commands.h -o commands.o


