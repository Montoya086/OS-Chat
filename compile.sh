gcc client.c chat.pb-c.c -o client.o -lprotobuf-c
gcc server.c chat.pb-c.c -o server.o -lprotobuf-c