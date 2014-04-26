all: client server

client: uds_client.c
	$(CC) uds_client.c -o uds_client
server: uds_server.c
	$(CC) uds_server.c -o uds_server
clean:
	rm uds_client uds_server
