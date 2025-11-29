#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

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
#define BUFSIZE 4096*5
/*
     f. Returns the full content of /var/tmp/aesdsocketdata to the client as soon as the received data packet completes.
    You may assume the total size of all packets sent (and therefore size of /var/tmp/aesdsocketdata) will be less than the size of the root filesystem, 
    however you may not assume this total size of all packets sent will be less than the size of the available RAM for the process heap.
     g. Logs message to the syslog “Closed connection from XXX” where XXX is the IP address of the connected client.
     h. Restarts accepting connections from new clients forever in a loop until SIGINT or SIGTERM is received (see below).
     i. Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.
    Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received.
*/

// create socket filde descriptors
int sfd, cfd;

volatile sig_atomic_t quit_requested = 0; //quit flag set by signal handler
void signal_handler(int sig)
{
    //Do NOT call printf, syslog, close, unlink, or anything complex inside the handler — unsafe inside signals.
    quit_requested = 1;
    sig = sig;
}

void install_signal_handlers()
{
    // Register signal handlers catch ctrl+c and signterm for graceful shutdown
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;    //do not use SA_RESTART

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // signal(SIGINT, signal_handler);
    // signal(SIGTERM, signal_handler);
}

int setup_tcp_server_socket()
{
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

    //configure address
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr)); //initiatlizes the struct with 0 to avoide garbage data
    server_addr.sin_family = AF_INET; //IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; //listen on all network interfaces - mordern/common
    // server_addr.sin_addr.s_addr = htonl(INADDR_ANY);//listen on all network interfaces - portable  for old code
    server_addr.sin_port = htons(PORT); //port 9000 (network byte order)

    if(bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) //bind the socket to the port
    {
        perror("bind failed!");
        close(sfd);
        return -1;
    }
    else printf("Socket bound to port %d successfully\n", PORT);

    // Listens for and accepts a connection
    if(listen(sfd, LISTEN_BACKLOG) == -1)
    {
        perror("listen failed!");
        close(sfd);
        return -1;
    }
    else printf("listening for connection...\n");

    return 0;
}



// // int no_of_recv_packets = 0;
int process_packets(ssize_t bytes_rcv, char recv_buffer[])
{
    //temporal storage for a complete packet
    // int * packet_buffer = malloc(sizeof(char) * 4096); //1024 char bytes
    // char packet_buffer[16384]; //temporal storage for a complete packet
    char *packet_buffer = NULL;
    size_t packet_size = 0;

    size_t packet_pos = 0;
    static int no_of_recv_packets = 0; // static makes it retain valu between calls
    

    FILE *fd;    
    fd = fopen(FILE_PATH, "a+"); //open in append rd wr mode 
    if(fd == NULL)
    {
        perror("failed to open file");
        packet_pos = 0;
        // break;
    }

    for(ssize_t i = 0; i < bytes_rcv; i++)
    {
        // Expand buffer if needed
        if (packet_pos + 1 >= packet_size) {
            packet_size = (packet_size == 0) ? 1024 : packet_size * 2;
            packet_buffer = realloc(packet_buffer, packet_size);
            if (!packet_buffer) {
                perror("realloc failed");
                free(packet_buffer);
                exit(1);
            }
            else printf("reallocated buff size: %ld\n", packet_size);
        }

        packet_buffer[packet_pos++] = recv_buffer[i];//extract each packets

        //check for newline
        if (recv_buffer[i] == '\n')
        {
            //append the complete packet to file
            fwrite(packet_buffer, 1, packet_pos, fd);
            fflush(fd);// saves data to memory
            printf("appended packet %d : %ld bytes recieved : %ld written to file\n", no_of_recv_packets++, bytes_rcv, packet_pos);
            if (packet_pos < packet_size) packet_buffer[packet_pos++] = '\0'; // null terminate packet buff

            //printf("packet_buffer content: %s\n", packet_buffer);
            //printf("packet_pos : %ld packet_size : %ld\n", packet_pos, packet_size);
            //reset packet buff for next packet
            packet_pos = 0;
            // memset(packet_buffer, 0, sizeof(packet_buffer));// clear packet buffer
        }
        else ;
    }

    printf("%ld bytes recieved and written to file\n", bytes_rcv);

    //send back file content to the client
    fseek(fd, 0, SEEK_SET);//go to the start of the file
    char send_buffer[BUFSIZE];
    size_t no_of_bytes_read;

    while((no_of_bytes_read = fread(send_buffer, 1, sizeof(send_buffer), fd)) > 0)
    {
        send(cfd, send_buffer, no_of_bytes_read, 0);
    }
    
    free(packet_buffer);
    fclose(fd); //close file

    return 0;
}

void handle_client(int cfd, char *client_ip, int client_port)
{
    //Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist.
    //Your implementation should use a newline to separate data packets received.  In other words a packet is considered complete when a newline character is found in the input receive stream, and each newline should result in an append to the /var/tmp/aesdsocketdata file.
    //You may assume the data stream does not include null characters (therefore can be processed using string handling functions).
    //You may assume the length of the packet will be shorter than the available heap size.  In other words, as long as you handle malloc() associated failures with error messages you may discard associated over-length packets.
    
    char recv_buffer[BUFSIZE]; //
    ssize_t bytes_rcv;
    size_t total_bytes_rcv = 0;
    //recieve loop
    while(!quit_requested)
    {
        while ((bytes_rcv = recv(cfd, recv_buffer, sizeof(recv_buffer), 0)) > 0)
        {
            syslog(LOG_INFO, "Recieved %s (%zd bytes) from %s:%d", recv_buffer, bytes_rcv, client_ip, client_port);
            //printf("Recieved %s (%zd bytes) from %s:%d\n", recv_buffer, bytes_rcv, client_ip, client_port);
            //total_bytes_rcv += bytes_rcv;
            //printf("Total bytes recieved : %zu\n", total_bytes_rcv);

            process_packets(bytes_rcv, recv_buffer);            
            //send back file content to the client
        }
        printf("Total bytes recieved : %zu\n", total_bytes_rcv);
        // // send(cfd, recv_buffer, bytes_rcv, 0); //echo recieved data 
        if (bytes_rcv < 0){
            //error or connecction close or termination signal recieved
            if(errno == EINTR && quit_requested)
                break; //
                    
            perror("recieve"); 
            break;
        }
        else if(bytes_rcv == 0)//remote connection close
        {
            printf("client %s:%d disconneted\n", client_ip, client_port);
            syslog(LOG_INFO, "Closed connection from %s:%d", client_ip, client_port);
            break;
        }
    }
}


void send_file_to_client()
{
    /*send back file content to the client*/
    char send_buffer[BUFSIZE];
    size_t no_of_bytes_read, sent_byte; 
    
    FILE *fd = fopen(FILE_PATH, "r");
    if(fd == NULL)
    {
        perror("failed to open file");
    }
    while((no_of_bytes_read = fread(send_buffer, 1, sizeof(send_buffer), fd)) > 0)
    {
        if((sent_byte = send(cfd, send_buffer, no_of_bytes_read, 0)) < 0)
        {
            if (errno == EINTR) continue;
            perror("send");
        }
    }
    fclose(fd); //close file
}



void recieve_packets(int cfd, char *client_ip, int client_port)
{
    //Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist.
    //Your implementation should use a newline to separate data packets received.  In other words a packet is considered complete when a newline character is found in the input receive stream, and each newline should result in an append to the /var/tmp/aesdsocketdata file.
    //You may assume the data stream does not include null characters (therefore can be processed using string handling functions).
    //You may assume the length of the packet will be shorter than the available heap size.  In other words, as long as you handle malloc() associated failures with error messages you may discard associated over-length packets.
    
    char recv_buffer[BUFSIZE]; // 4kb buffer
    ssize_t bytes_rcv;
    size_t total_bytes_rcv = 0;

    char *packet_buffer = NULL;
    size_t packet_size = 0;

    size_t packet_pos = 0;
    static int no_of_recv_packets = 0; // static makes it retain valu between calls
    

    FILE *fd;    
    fd = fopen(FILE_PATH, "a+"); //open in append rd wr mode 
    if(fd == NULL)
    {
        perror("failed to open file");
        packet_pos = 0;
        // break;
    }

    //recieve loop
    while(!quit_requested)
    {
        while ((bytes_rcv = recv(cfd, recv_buffer, sizeof(recv_buffer), 0)) > 0)
        {
            syslog(LOG_INFO, "Recieved %s (%zd bytes) from %s:%d", recv_buffer, bytes_rcv, client_ip, client_port);
            //printf("Recieved %s (%zd bytes) from %s:%d\n", recv_buffer, bytes_rcv, client_ip, client_port);
            //total_bytes_rcv += bytes_rcv;
            //printf("Total bytes recieved : %zu\n", total_bytes_rcv);

            //process_packets(bytes_rcv, recv_buffer);            
            for(ssize_t i = 0; i < bytes_rcv; i++)
            {
                // Expand buffer if needed
                if (packet_pos + 1 >= packet_size) {
                    packet_size = (packet_size == 0) ? 1024 : packet_size * 2;
                    packet_buffer = realloc(packet_buffer, packet_size);
                    if (!packet_buffer) {
                        perror("realloc failed");
                        free(packet_buffer);
                        exit(1);
                    }
                    else printf("reallocated buff size: %ld\n", packet_size);
                }

                packet_buffer[packet_pos++] = recv_buffer[i];//extract each packets

                //check for newline
                if (recv_buffer[i] == '\n')
                {
                    //append the complete packet to file
                    fwrite(packet_buffer, 1, packet_pos, fd);
                    fflush(fd);// saves data to memory
                    printf("appended packet %d : %ld bytes recieved : %ld written to file\n", no_of_recv_packets++, bytes_rcv, packet_pos);
                    if (packet_pos < packet_size) packet_buffer[packet_pos++] = '\0'; // null terminate packet buff

                    //printf("packet_buffer content: %s\n", packet_buffer);
                    //printf("packet_pos : %ld packet_size : %ld\n", packet_pos, packet_size);
                    //reset packet buff for next packet
                    packet_pos = 0;
                    // memset(packet_buffer, 0, sizeof(packet_buffer));// clear packet buffer
                }
                else ;
            }
            //send back file content to the client
            send_file_to_client();
        }
        printf("Total bytes recieved : %zu\n", total_bytes_rcv);
        
        // // send(cfd, recv_buffer, bytes_rcv, 0); //echo recieved data 
        if (bytes_rcv < 0){
            //error or connecction close or termination signal recieved
            if(errno == EINTR && quit_requested)
                break; //
                    
            perror("recieve"); 
            break;
        }
        else if(bytes_rcv == 0)//remote connection close
        {
            printf("client %s:%d disconneted\n", client_ip, client_port);
            syslog(LOG_INFO, "Closed connection from %s:%d", client_ip, client_port);
            break;
        }
    }
}


void shutdown_server_and_clean_up()
{
    if (quit_requested == 1)
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        printf("\nServer is shutting down\n");
        
        if(cfd != -1) close(cfd);
        if(sfd != -1) close(sfd);

        //close and delete /var/tmp/aesdsocketdata. 
        if(unlink(FILE_PATH) == 0){
            printf("deleted %s\n", FILE_PATH);
        }
        else
        {
            perror("unlink error");
        }
        
        closelog();
        exit(0);//terminate program
    }
}
int daemonize()
{
    /*  Fork the process
        - Parent exits.
        - Child continues running in the background.
        - This ensures the daemon is not a process group leader (important for setsid). 
        */
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        /* parent exits */
        exit(EXIT_SUCCESS);
    }

    /* child continues */
    /*
        Create a new session (setsid)
        - Detaches from terminal
        - Makes the child become session leader
        - Creates a new process group
    */
    if (setsid() < 0) {
        syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    /*
        Fork again. recommended to prevent the daemon from ever again acquiring a terminal.
    */
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        /* parent exits */
        exit(EXIT_SUCCESS);
    }

    /*
    set file permission mask
    Most daemons use umask(0) to allow free file creation.
    */
    umask(0);

    /* Change working directory 
        Usually / so daemon doesn’t block unmounting filesystems.
    */
    if (chdir("/var/tmp") < 0) {
        syslog(LOG_ERR, "chdir failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Close standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Redirect stdio to /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }

    openlog("TCP Server(aesdsocket_daemon)", LOG_PID|LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon started!");

    return 0;
}


int main(int argc, char *argv[])
{
    bool daemon_mode  = false;
    if ((argc ==2) && (strcmp(argv[1], "-d") == 0))
    {
        daemon_mode = true;
    }

    //open connection to syslog
    openlog("TCP Server(aesdsocket)", LOG_PID|LOG_CONS, LOG_USER ); 
    
    install_signal_handlers();

    //Opens a stream socket bound to port 9000, failing and returning -1 if any of the socket connection steps fail.
    // int sfd, cfd;

    syslog(LOG_INFO, "Starting TCP server on port %d", PORT);
    if(setup_tcp_server_socket() < 0) shutdown_server_and_clean_up();

    if (daemon_mode == true) daemonize();

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    //accept loop
    while(!quit_requested)
    {
        //accept an incomming connection
        cfd = accept(sfd, (struct sockaddr *)&client_addr, &client_len);
        if(cfd < 0)
        {
            if(errno == EINTR && quit_requested)
                break; //
            
            perror("Accept failed!");
            continue;
        }
        
        char *client_ip = inet_ntoa(client_addr.sin_addr);
        int client_port = ntohs(client_addr.sin_port);
        printf("Accepting connection from %s:%d\n", client_ip, client_port);
        syslog(LOG_INFO, "Accepted connection from %s:%d", client_ip, client_port);

        handle_client(cfd, client_ip, client_port);        
    }
    //clean up and shutdown server/client connection
    shutdown_server_and_clean_up();
}