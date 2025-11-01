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
#define LISTEN_BACKLOG 10   //defines how may pending connections can queue before they are refused
#define FILE_PATH "/var/tmp/aesdsocketdata"
/*
     f. Returns the full content of /var/tmp/aesdsocketdata to the client as soon as the received data packet completes.

    You may assume the total size of all packets sent (and therefore size of /var/tmp/aesdsocketdata) will be less than the size of the root filesystem, however you may not assume this total size of all packets sent will be less than the size of the available RAM for the process heap.

     g. Logs message to the syslog “Closed connection from XXX” where XXX is the IP address of the connected client.

     h. Restarts accepting connections from new clients forever in a loop until SIGINT or SIGTERM is received (see below).

     i. Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.

    Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received.
*/

// create socket filde descriptors
int sfd, cfd;

void signal_handler(int sig)
{
    printf("\nRecieved signal %d closing socket and exiting...\n", sig);
    syslog(LOG_INFO, "Caught signal, existing");
    close(sfd);
    exit(0);
}

int main(int argc, char *argv[])
{
    //Opens a stream socket bound to port 9000, failing and returning -1 if any of the socket connection steps fail.
    // int sfd, cfd;
    struct sockaddr_in server_addr, client_addr;
    char recv_buffer[16384]; // 16mb buffer

    //open connection to syslog
    openlog("TCP Server(aesdsocket)", LOG_PID|LOG_CONS, LOG_USER );
    syslog(LOG_INFO, "Starting TCP server on port %d", PORT);

    // catch ctrl+c and signterm for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    //create socket
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sfd == -1){
        perror("socket failed!");
        // syslog(LOG_ERR, "Socket creation failed: %s" strerror(errno));
        return -1;
    }

    //use the setsocketopt() to allow address/port reuse, setting timeout, enabling keepalive adjusting buffer sizes etc
    int opt = 1;
    if(setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) <0)
    {
        perror("setsocketopt(SO_REUSEPORT) failed");
        close(sfd);
        return -1;
    }
    if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsocketopt(SO_REUSEADDR) failed");
        close(sfd);
        return -1;
    }

    // /* 
    // configure server timeout
    // */
    // struct timeval timeout;
    // timeout.tv_sec = 20;
    // timeout.tv_usec = 0;
    // if(setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    // {
    //     perror("setsocketopt(SO_RCVTIMEO) failed");
    //     close(sfd);
    //     return -1;
    // }


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

    socklen_t client_len = sizeof(client_addr);
    while(1)
    {
        //accept an incomming connection
        cfd = accept(sfd, (struct sockaddr *)&client_addr, &client_len);
        if(cfd == -1)
        {
            perror("Accept failed!");
            continue;
        }
        //else 
        {
            char *client_ip = inet_ntoa(client_addr.sin_addr);
            int client_port = ntohs(client_addr.sin_port);
            printf("Accepting connection from from %s:%d\n", client_ip, client_port);
            syslog(LOG_INFO, "Accepted connection from %s:%d", client_ip, client_port);

            //Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist.
            //Your implementation should use a newline to separate data packets received.  In other words a packet is considered complete when a newline character is found in the input receive stream, and each newline should result in an append to the /var/tmp/aesdsocketdata file.
            // You may assume the data stream does not include null characters (therefore can be processed using string handling functions).
            // You may assume the length of the packet will be shorter than the available heap size.  In other words, as long as you handle malloc() associated failures with error messages you may discard associated over-length packets.
                
            char packet_buffer[1024]; //temporal storage for a complete packet
            size_t packet_pos = 0;
            int no_of_recv_packets = 0; //
            FILE *fd; //file descriptor

            while(1)
            {
                size_t bytes_rcv = recv(cfd, recv_buffer, sizeof(recv_buffer), 0);//recieve byte
                syslog(LOG_INFO, "Recieved %s from %s:%d", recv_buffer, client_ip, client_port);
                // send(cfd, recv_buffer, bytes_rcv, 0); //echo recieved data 
                if (bytes_rcv < 0){
                    //error or connecction close
                    if(errno ==EAGAIN || errno == EWOULDBLOCK) //THis errono was set after tieout
                    { // < 0
                        printf("Timeout waiting waiting for client %s:%d", client_ip, client_port);
                        syslog(LOG_INFO, "imeout waiting waiting for client %s:%d", client_ip, client_port);
                    }
                    // else{ // == 0
                    //     // perror("remote connection closed");
                    //     printf("client %s:%d disconneted\n", client_ip, client_port);
                    // }
                    break;
                }
                else if(bytes_rcv == 0)
                {//remote connection close
                    printf("client %s:%d disconneted\n", client_ip, client_port);
                    break;
                }
                else if(bytes_rcv > 0)
                {
                    //recieve all packets
                    for(size_t i = 0; i < bytes_rcv; i++)
                    {
                        packet_buffer[packet_pos++] = recv_buffer[i];//extract each packets

                        //check for newline
                        if (recv_buffer[i] == '\n')
                        {
                            fd = fopen(FILE_PATH, "a+"); //open in append rd wr mode 
                            if(fd == NULL)
                            {
                                perror("failed to open file");
                                // break;
                            }

                            //append the complete packet to file
                            fwrite(packet_buffer, 1, packet_pos, fd);
                            fflush(fd);// saves data to memory
                            printf("appended packet %d\n", no_of_recv_packets++);

                            //reset pacct buff for next packet
                            packet_pos = 0;
                            memset(packet_buffer, 0, sizeof(packet_buffer));
                        }
                    }
                    // bytes_rcv = 0;//
                    // //send back file content to the client
                    fseek(fd, 0, SEEK_SET);//go to the start of the file
                    char send_buffer[1024];
                    size_t no_of_bytes_read;

                    while((no_of_bytes_read = fread(send_buffer, 1, sizeof(send_buffer), fd)) > 0)
                    {
                        send(cfd, send_buffer, no_of_bytes_read, 0);
                    }
                    fclose(fd); //close file
                    //close client
                    // if (cfd >= 0)
                    // {
                    //     close(cfd);
                    //     syslog(LOG_INFO, "Closed connection from %s", client_ip);
                    // }
                }
            }
        }
        // recv_buffer[bytes_rcv] = '\0';
        // fwrite(recv_buffer, 1, bytes_rcv, fd);
    }
    // pause();//waiting for connection from client
    // close(sfd);
    syslog(LOG_INFO, "Server is shutting down");
    closelog();

    return 0;
}

