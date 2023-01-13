//Daniel Barroso Rocio y Jonathan Martínez Pérez
//Socket Programming



#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <iostream>
#include <dirent.h>

#include "common.h"

#include "ClientConnection.h"
#include "FTPServer.h"


ClientConnection::ClientConnection(int s) {

    control_socket = s;
    // Check the Linux man pages to know what fdopen does.
    fd = fdopen(s, "a+");
    if (fd == NULL){
        std::cout << "Connection closed" << std::endl;

        fclose(fd);
        close(control_socket);
        ok = false;
        return ;
    }

    ok = true;
    data_socket = -1;
};


ClientConnection::~ClientConnection() {
 	fclose(fd);
	close(control_socket);

}


int connect_TCP( uint32_t address,  uint16_t  port) {
     // Implement your code to define a socket here
    struct sockaddr_in sin;
	int s = 0;
	s = socket(AF_INET,SOCK_STREAM, 0);
	if(s < 0)
		errexit("No puedo crear el socket:%s\n", strerror(errno));

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = address;
	sin.sin_port = htons(port);
//	if(bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0){ ///SOLAMENTE ÚTIL SI EL SOCKET VA A ESCUCHAR ALGO.Asocia el socket a un puerto
//        errexit("No puedo hacer el bind con el puerto: %s\n",strerror(errno));
//        s = -2;
//	}
	if (connect(s,(struct sockaddr *)&sin, sizeof(sin)) < 0){ //Espera a SYN-ACK server
		errexit("Fallo en el connect: %s\n", strerror(errno));
		s = -1;
    }
	return s;

}




void ClientConnection::stop() {
    close(data_socket);
    close(control_socket);
    _parar = true;

}





#define COMMAND(cmd) strcmp(command, cmd)==0

// This method processes the requests.
// Here you should implement the actions related to the FTP commands.
// See the example for the USER command.
// If you think that you have to add other commands feel free to do so. You
// are allowed to add auxiliary methods if necessary.

void ClientConnection::WaitForRequests() {
    if (!ok) {
	 return;
    }

    int s = 0;
    uint32_t address = 0;
    uint16_t port = 0;
    fprintf(fd, "220 Service ready\n");

    while(!_parar) {

      fscanf(fd, "%s", command);
      if (COMMAND("USER")) {
	    fscanf(fd, "%s", arg);
	    if( !std::string(arg).compare("user") )
            fprintf(fd,"331 Username ok, need password\n");
 	    else{
            fprintf(fd,"430 Invalid username or password\n");
            this->ok = false;
            _parar = true;
 	    }
      }


      else if (COMMAND("PORT")) {
        uint32_t a3 = 0,a2 = 0,a1 = 0,a0 = 0,p1 = 0,p0 = 0;
        fscanf(fd, "%d,%d,%d,%d,%d,%d", &a3,&a2,&a1,&a0,&p1,&p0);
        address = a0<<24|a1<<16|a2<<8|a3;
        port = p1<<8|p0;

        data_socket = connect_TCP(address,port);
        if(data_socket < 0) fprintf(fd,"501 .Error de sintaxis en parámetros o argumentos.");
        else fprintf(fd,"200 OK.\n");


      }
      else if (COMMAND("PASS")) {
        fscanf(fd, "%s", arg);
        if(!std::string(arg).compare("1234")){
            fprintf(fd,"230 User logged in,proceed. Logged out if appropriate\n");
        }
        else{
            fprintf(fd,"430 Invalid username or password\n");
            this->ok = false;
            _parar = true;
        }
      }
      else if (COMMAND("PASV")) {
        s = define_socket_TCP(0);
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        socklen_t saLen = sizeof(sin);
        getsockname(s,(struct sockaddr *)&sin, &saLen);

        //data_socket = connect_TCP(address,port);
        struct sockaddr_in sin2;
        socklen_t saLen2 = sizeof(sin2);
        printf("\nPuerto asignado: %d", sin.sin_port);
        printf("\nDir asignada: %d", sin.sin_addr.s_addr);
        fprintf(fd,"227 Entering Passive Mode (127,0,0,1,%d,%d).\n",sin.sin_port&0xFF,sin.sin_port>>8);
        fflush(fd);
        data_socket = accept(s, (struct sockaddr *)&sin2, &saLen2);
      }

      else if (COMMAND("STOR") ) {
        fscanf(fd,"%s",arg);
        FILE* file = fopen(arg,"wb");
        if(file == nullptr) fprintf(fd,"451 .Requested action aborted. Local error in processing.\n");
        else {
            fprintf(fd,"150 File status okay; about to open data connection.\n");
            fflush(fd);
            int bytesWritten = 0;
            char* buffer;
            do{ ///
                buffer = new char[MAX_BUFF];
                bytesWritten = recv(data_socket,buffer,MAX_BUFF,0);
                fwrite(buffer,1, bytesWritten, file);
                delete buffer;            }while(bytesWritten);
            fclose(file);
            fprintf(fd,"226 Closing data connection.\n");
            fflush(fd);
        }
        close(data_socket);
        data_socket = 0;

      }
      else if (COMMAND("SYST")) { ///Despues de password sucess
         fprintf(fd,"215 UNIX Type: L8.\n");
      }
      else if (COMMAND("TYPE")) {
        fscanf(fd, "%s", arg); //recogemos arg 'I' (=trassnferencia binaria)
        fprintf(fd,"200 OK.\n");
      }
      else if (COMMAND("RETR")) {
        fscanf(fd,"%s",arg);
        FILE* file = fopen(arg,"rb");
        if(file == nullptr) fprintf(fd,"451 Requested action aborted. Local error in processing.\n");
        else {
            //printf("Dentro de bucle para mandar\n");
            fprintf(fd,"150 File status okay; about to open data connection.\n");
            fflush(fd);
            int bytesRead = 0;
            //printf("Datasocket = %d\n", sfd);
            char* buffer;
            while(!feof(file)){
                buffer = new char[MAX_BUFF];
                bytesRead = fread(buffer,1,MAX_BUFF,file);
                send(data_socket,buffer,bytesRead,0);
                //printf("%d\n",bytesRead);
                //printf("Datasocket = %d\n", data_socket);
                delete buffer;            }
            fclose(file);
            //printf("Fuera del bucle\n");
            fprintf(fd,"226 Closing data connection.\n");
            //printf("Datasocket = %d\n", sfd);
            fflush(fd);
            close(data_socket);
        }
        data_socket = 0;
      }
      else if (COMMAND("QUIT")) {
        _parar = true;
        if(s != 0) close(s);
      }
      else if (COMMAND("LIST")) {
        //fscanf(fd,"%s",arg);
        DIR* dirp = opendir(".");
        dirent* dp = nullptr;
        fprintf(fd,"125 List started.OK\n");
        while ( (dp = readdir(dirp)) != nullptr ){
            std::string str = std::string(dp->d_name) + "\n";
            send(data_socket,str.data(), str.size(),0);
        }
        close(data_socket);
        fprintf(fd,"250 List completed succesfully.\n");
      }
      else  {
	    fprintf(fd, "502 Command not implemented.\n"); fflush(fd);
	    printf("Comando : %s %s\n", command, arg);
	    printf("Error interno del servidor\n");

      }

    }

    fclose(fd);


    return;

};
