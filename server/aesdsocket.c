#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <errno.h>
#include <sys/syslog.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define PORT 9000
//#define INADDR_ANY 0.0.0.0  // listen to all interfaces
#define LISTEN_BACKLOG 10   //defines how may pending connections can queue before they are refused



//d. Logs message to the syslog “Accepted connection from xxx” where XXXX is the IP address of the connected client. 

/*
     e. Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist.

    Your implementation should use a newline to separate data packets received.  In other words a packet is considered complete when a newline character is found in the input receive stream, and each newline should result in an append to the /var/tmp/aesdsocketdata file.

    You may assume the data stream does not include null characters (therefore can be processed using string handling functions).

    You may assume the length of the packet will be shorter than the available heap size.  In other words, as long as you handle malloc() associated failures with error messages you may discard associated over-length packets.

     f. Returns the full content of /var/tmp/aesdsocketdata to the client as soon as the received data packet completes.

    You may assume the total size of all packets sent (and therefore size of /var/tmp/aesdsocketdata) will be less than the size of the root filesystem, however you may not assume this total size of all packets sent will be less than the size of the available RAM for the process heap.

     g. Logs message to the syslog “Closed connection from XXX” where XXX is the IP address of the connected client.

     h. Restarts accepting connections from new clients forever in a loop until SIGINT or SIGTERM is received (see below).

     i. Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.

    Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received.
*/

// create socket filde descriptors
int sfd, cfd;

// void handle_sigint(int sig)
// {
//     printf("\nRecieved signal %d closing socket and exiting...\n", sig);
//     close(sfd);
//     exit(0);
// }
    

int main(int argc, char *argv[])
{
    //Opens a stream socket bound to port 9000, failing and returning -1 if any of the socket connection steps fail.
    // int sfd, cfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[1024];

    //open connection to syslog
    openlog("TCP Server(aesdsocket)", LOG_PID|LOG_CONS, LOG_USER );
    syslog(LOG_INFO, "Starting TCP server on port %d", PORT);

    //catch ctrl+c and signterm for graceful shutdown
    // signal(SIGINT, handle_sigint);
    // signal(SIGTERM, handle_sigint);

    //create socket
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sfd == -1){
        perror("socket failed!");
        // syslog(LOG_ERR, "Socket creation failed: %s" strerror(errno));
        return -1;
    }

    //use the setsocketopt() to allow address/port reuse, setting timeout, enablingi keepalive adjusting buffer sizes etc
    int opt = 1;
    if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsocketopt(SO_REUSEADDR) failed");
        close(sfd);
        return -1;
    }

    //configure address
    memset(&server_addr, 0, sizeof(server_addr)); //initiatlizes the struct with 0 to avoide garbage data
    server_addr.sin_family = AF_INET; //IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; //listen on all network interfaces - mordern/common
    //server_addr.sin_addr.s_addr = htonl(INADDR_ANY);//listen on all network interfaces - portable  for old code
    server_addr.sin_port = htons(PORT); //port 9000 (network byte order)

    if(bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) //bind the socket to the port
    {
        perror("bind failed!");
        close(sfd);
        return -1;
    }
    else printf("Socket opened and bound to port %d successfully\n", PORT);

    //  c. Listens for and accepts a connection
    if(listen(sfd, LISTEN_BACKLOG) == -1)
    {
        perror("listen failed!");
        close(sfd);
        return -1;
    }
    else printf("listening for connection...\n");

    //accept an incomming connection
    //socklen_t client_len = sizeof(client_addr);
    cfd = accept(sfd, (struct sockaddr *)&client_addr, (socklen_t *) sizeof(client_addr));
    if(cfd == -1)
    {
        perror("Accept failed!");
        close(sfd);
        return -1;
    }
    else printf("Accepting connection...\n");

    while(1)
        pause();//waiting for connection from client
    
    // close(sfd);

    return 0;
}

