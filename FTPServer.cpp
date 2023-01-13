//Daniel Barroso Rocio y Jonathan Martínez Pérez
//Socket Programming

#include <cerrno>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

 #include <unistd.h>


#include <pthread.h>

#include <list>

#include <iostream>

#include "common.h"
#include "FTPServer.h"
#include "ClientConnection.h"


int define_socket_TCP( int port) {
   // Include the code for defining the socket.
	struct sockaddr_in sin;
	int s;
	s = socket(AF_INET,SOCK_STREAM, 0);
	if(s < 0)
		errexit("No puedo crear el socket:%s\n", strerror(errno));

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(port);
	if(bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		errexit("No puedo hacer el bind con el puerto: %s\n",strerror(errno));
	if (listen(s, 5) < 0) //Espera a SYN-ACK server
		errexit("Fallo en el listen: %s\n", strerror(errno));
	return s;
}

// This function is executed when the thread is executed.
void* run_client_connection(void *c) {
    ClientConnection *connection = (ClientConnection *)c;
    connection->WaitForRequests();

    return NULL;
}



FTPServer::FTPServer(int port) {
    this->port = port;
}


// Parada del servidor.
void FTPServer::stop() {
    close(msock);
    shutdown(msock, SHUT_RDWR);

}


// Starting
void FTPServer::run() {

    struct sockaddr_in fsin;
    int ssock;
    socklen_t alen = sizeof(fsin);
    char h0 = 127;
    char h1 = 0;
    char h2 = 0;
    char h3 = 1;
    uint32_t addr = h0<<24|h1<<16|h2<<8|h3;
    std::cout << addr << std::endl;
    msock = define_socket_TCP(port);  // This function must be implemented by you.
    while (1) {
        pthread_t thread;
        ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
        if(ssock < 0)
            errexit("Fallo en el accept: %s\n", strerror(errno));

        ClientConnection *connection = new ClientConnection(ssock);


        pthread_create(&thread, NULL, run_client_connection, (void*)connection);

    }

}
