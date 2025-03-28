# OS-Chat

## Protocol
In order to compile the protocol, you need to install protobuf. You can do that with the following commands:
```
sudo apt-get update
sudo apt-get install protobuf-c-compiler libprotobuf-c-dev
```
Once you installed protobuff, you can now compile the protocol via:
```
protoc --c_out=. <chat>.proto 
```

## Usage
In order to compile use the following:
```
gcc <server/client>.c chat.pb-c.c -o <server/client>.o -lprotobuf-c
```
OR
```
bash compile.sh
```

In order to run the server use the following:
```
./server.o <port>
```
In order to run a client use:
```
./client.o <server_address> <server_port> <username>
```
In a local environment, <server_address> is "0.0.0.0".
