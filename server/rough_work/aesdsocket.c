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
#include <pthread.h>
#include <time.h>

#define PORT 9000
#define LISTEN_BACKLOG 50   //defines how may pending connections can queue before they are refused
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define RECV_BUF_SIZE 2048 //
#define SEND_BUF_SIZE 2048 //

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

//define thread
pthread_t client_thread, time_thread;
//define mutex
pthread_mutex_t mutex;

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

void shutdown_server_and_clean_up()
{
    if (quit_requested == 1)
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        printf("\nServer is shutting down\n");

        if(cfd != -1) {
            //signals end of data transmission to client
            if(shutdown(cfd, SHUT_WR) == -1) {
                perror("cfd shutdown failed");
            }
            close(cfd);
        }
        if(sfd != -1) {
            // Stop the listening socket entirely during server shutdown
            // This prevents new connections while existing ones are cleaned up.
            if(shutdown(sfd, SHUT_RDWR) == -1) {
                perror("sfd shutdown failed");
            }
            close(sfd);
        }

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
    else printf("Socket bound to port %d successfully\n", ntohs(server_addr.sin_port));

    // Listens for and accepts a connection
    if(listen(sfd, LISTEN_BACKLOG) == -1)
    {
        perror("listen failed!");
        close(sfd);
        return -1;
    }
    else printf("listening for connection...\n");

    return 0;//
}

      
ssize_t write_to_file(FILE *fd, char data_str[], int data_len)
{
    /****** create lock *******/
	pthread_mutex_lock(&mutex);

    // // FILE *fd; 
    // fd = fopen(FILE_PATH, "a+"); //open in append rd wr mode 
    // if(fd == NULL)
    // {
    //     perror("failed to open file");
    //     // packet_pos = 0;
    //     // break;
    // }
    ssize_t write_s = fwrite(data_str, 1, data_len, fd);
    fflush(fd);// saves data to memory    
    // fclose(fd); //close file

    /***** release lock *******/
	pthread_mutex_unlock(&mutex);

    return write_s;
}


int process_packets(ssize_t bytes_rcv, char recv_buffer[])
{
    //temporal storage for a complete packet
    char packet_buffer[RECV_BUF_SIZE]; //temporal storage for a complete packet
    // char *packet_buffer = NULL; // dynamic allocation

    size_t packet_pos = 0;
    static size_t packet_counter = 0, partial_packet_counter = 0; // static makes it retain valu between calls
    
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
        // /* Protect against available heap buffer overflow */
        if ((packet_pos > RECV_BUF_SIZE) || (bytes_rcv > RECV_BUF_SIZE)) 
        {
            syslog(LOG_ERR, "packet exceeds buffer size, discarding... Packet_pos : %ld\nbytes_rcv : %ld\npacket_size : %ld\n", packet_pos, bytes_rcv, packet_pos);
            printf("packet exceeds buffer size, discarding...\n");
            printf("Packet_pos : %ld\nbytes_rcv : %ld\npacket_size : %ld\n", packet_pos, bytes_rcv, packet_pos);
            //free(packet_buffer); for dynamic allocation
            packet_pos = 0;
            close(cfd);
            break;
        }

        packet_buffer[packet_pos++] = recv_buffer[i];//extract each packets

        // /****** create lock *******/
	    // pthread_mutex_lock(&mutex);

        //check for newline
        if (recv_buffer[i] == '\n')
        {
            //append the complete packet to file
            // ssize_t write_s = fwrite(packet_buffer, 1, packet_pos, fd);
            // fflush(fd);// saves data to memory
            ssize_t write_s = write_to_file(fd, packet_buffer, packet_pos);

            printf("[Packet %ld] : Size = %ld byte \n%ld byte written to file successfully\n", ((packet_counter++)+1), packet_pos, write_s);//packet_pos = packet size            
            //reset packet buff for next packet
            packet_pos = 0;
            // memset(packet_buffer, 0, sizeof(packet_buffer));// clear packet buffer

            //send back file content to the client
            printf("Sending back file content to client...\n");   
            fseek(fd, 0, SEEK_SET);//go to the start of the file
                            
            char send_buffer[SEND_BUF_SIZE];
            ssize_t no_of_bytes_read;
            while((no_of_bytes_read = fread(send_buffer, 1, sizeof(send_buffer), fd)) > 0)
            {
                send(cfd, send_buffer, no_of_bytes_read, 0);
            }
        }
        else if(i == (bytes_rcv-1))
        {
            printf("Large packet!!, Handling packets in chunks to prevent buffer overflow...\n");
            //append partial packet to file without new line
            // ssize_t write_s = fwrite(packet_buffer, 1, packet_pos, fd);
            // fflush(fd);// saves data to memory;
            ssize_t write_s = write_to_file(fd, packet_buffer, packet_pos);
            packet_pos = 0;
            printf("[%ld]:%ld byte written to file\n\n", ((partial_packet_counter++)+1), write_s);
            // printf("\n----endof buffer reached----\n i = %ld, bytes_rcv = %ld\n", i++, bytes_rcv);
        }

        // /***** release lock *******/
	    // pthread_mutex_unlock(&mutex);
    }

    //free(packet_buffer); for dynamic allocation
    fclose(fd); //close file

    return 0;
}


void handle_client(int cfd, char *client_ip, int client_port)
{
    //Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist.
    //Your implementation should use a newline to separate data packets received.  In other words a packet is considered complete when a newline character is found in the input receive stream, and each newline should result in an append to the /var/tmp/aesdsocketdata file.
    //You may assume the data stream does not include null characters (therefore can be processed using string handling functions).
    //You may assume the length of the packet will be shorter than the available heap size.  In other words, as long as you handle malloc() associated failures with error messages you may discard associated over-length packets.
    
    char recv_buffer[RECV_BUF_SIZE]; //
    ssize_t bytes_rcv;
    size_t total_bytes_rcv = 0;

    printf("Recieving packets...\n\n");
    //recieve loop
    while(!quit_requested)
    {
        while ((bytes_rcv = recv(cfd, recv_buffer, sizeof(recv_buffer), 0)) > 0)
        {
            // syslog(LOG_INFO, "Recieved %zd bytes from %s:%d", bytes_rcv, client_ip, client_port);
            // printf("Recieved => %s (%zd bytes) from %s:%d\n", recv_buffer, bytes_rcv, client_ip, client_port);
            total_bytes_rcv += bytes_rcv;
            printf("--- From %s:%d ----\n", client_ip, client_port);
            printf("Bytes recieved : %zu byte\n", bytes_rcv);
            process_packets(bytes_rcv, recv_buffer); 
            printf("---Total bytes recieved : %zu byte----\n\n", total_bytes_rcv);
        }
                
        // send(cfd, recv_buffer, bytes_rcv, 0); //echo recieved data 
        if (bytes_rcv < 0)
        {
            //error or connecction close or termination signal recieved
            if(errno == EINTR && quit_requested)
                break; //
                    
            perror("recieve error"); 
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

    /****** create lock *******/
	pthread_mutex_lock(&mutex);

    char send_buffer[SEND_BUF_SIZE];
    ssize_t no_of_bytes_read, sent_byte; 
    
    FILE *f = fopen(FILE_PATH, "r");
    if(f == NULL)
    {
        perror("failed to open file");
    }
    while((no_of_bytes_read = fread(send_buffer, 1, sizeof(send_buffer), f)) > 0)
    {
        if((sent_byte = send(cfd, send_buffer, no_of_bytes_read, 0)) < 0)
        {
            if (errno == EINTR) continue;
            perror("send error");
        }
    }
    fclose(f); //close file

    /***** release lock *******/
	pthread_mutex_unlock(&mutex);
}


#define PIDFILE "/var/tmp/aesdsocket.pid" //"/var/run/aesdsocket.pid"
void remove_pidfile(void)
{
    unlink(PIDFILE);
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
     
    // open pid file
    FILE *fp = fopen(PIDFILE, "w");
    if (fp == NULL) {
        perror("Failed to open PIDFILE");
        exit(EXIT_FAILURE);
    }

    char pid_str[10];
    int len = snprintf(pid_str, sizeof(pid_str), "%d\n", getpid());
    // write pid to file
    if (fwrite(pid_str, 1, len, fp) != (size_t)len) {
        perror("Failed to write PIDFILE");
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    fclose(fp);
    // cleanup on exit
    atexit(remove_pidfile);

    /*
    final daemon process
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


struct client_socket_addr_in
{
    int cfd;
    int client_port;
    char *client_ip;    
};  //struct client_socket_addr_in *p_client_socket_addr;

// function to be executed in a thread
void *handle_client_thread(void *p_client_addr)
{
    if (!p_client_addr)
    return NULL;

    struct client_socket_addr_in *p_client_socket_addr = (struct client_socket_addr_in *)p_client_addr;

    handle_client(p_client_socket_addr->cfd, p_client_socket_addr->client_ip, ntohs(p_client_socket_addr->client_port));
    close(p_client_socket_addr->cfd);
    free(p_client_addr);
    return NULL;
}

//timestamp thread function
void *write_timestamp(void* arg)
{
    (void)arg;
    time_t current_time;
    struct tm time_info;
    char time_str[80];
    // FILE *fd = (FILE *) arg;

    while(!quit_requested)
    {
        //get cuurent time
        time(&current_time);
        localtime_r(&current_time, &time_info);
        strftime(time_str, sizeof(time_str), "timestamp:%a, %d %b %Y %T %z\n", &time_info);

        //write time str to file
        write_to_file(time_str, strlen(time_str));//writes exaxt string up to the position of the null char
        // printf("%s", time_str);

        // Interrupt-responsive sleep
        for(int i=0; i<10 && !quit_requested; i++) sleep(1);
    }
    return NULL;
}


int main(int argc, char *argv[])
{
    bool daemon_mode  = false;
    if ((argc ==2) && (strcmp(argv[1], "-d") == 0))
    {
        daemon_mode = true;
    }

    //create client thread
    int rc = pthread_create(&time_thread, NULL, write_timestamp, NULL);
    if (rc != 0){
        // free(time_thread_arg);
        return false;
    }
    
    //init mutex
	pthread_mutex_init(&mutex, NULL);

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
            
            perror("\nAccept failed!");
            continue;
        }
        
        //extract clint info
        //save in struct to be passed to the thread function (handle connection)      
        struct client_socket_addr_in *p_client_socket_addr = malloc(sizeof(struct client_socket_addr_in));//malloc to alloc a mem to prevent cfd from being overwritten by other client thread
        p_client_socket_addr->client_ip = inet_ntoa(client_addr.sin_addr);
        p_client_socket_addr->client_port = ntohs(client_addr.sin_port);
        p_client_socket_addr->cfd = cfd;
        
        printf("\nAccepting connection from %s:%d\n", p_client_socket_addr->client_ip, p_client_socket_addr->client_port);
        syslog(LOG_INFO, "Accepted connection from %s:%d", p_client_socket_addr->client_ip, p_client_socket_addr->client_port);

        // handle_client(p_client_socket_addr->cfd, p_client_socket_addr->client_ip, ntohs(p_client_socket_addr->client_port));
        
        //create client thread
        int rc = pthread_create(&client_thread, NULL, handle_client_thread, p_client_socket_addr);
        if (rc != 0){
    	    free(p_client_socket_addr);
    	    return false;
        }
        // pthread_join(thread, NULL);
    }
    //clean up and shutdown server/client connection
    // free(p_client_socket_addr);
    if(pthread_join(client_thread, NULL) != 0) perror("\n client_thread error");
    if(pthread_join(time_thread, NULL) != 0) perror("\n time_thread error");
    // pthread_mutex_destroy(&mutex);
    shutdown_server_and_clean_up();
}