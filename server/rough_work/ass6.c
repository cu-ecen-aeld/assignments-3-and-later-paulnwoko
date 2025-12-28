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
#include <sys/queue.h> // Added for linked list

#define PORT 9000
#define LISTEN_BACKLOG 50   //defines how may pending connections can queue before they are refused
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define RECV_BUF_SIZE 2048 //
#define SEND_BUF_SIZE 2048 //

// 1. Updated struct to support Singly Linked List
struct client_socket_addr_in
{
    int cfd;
    int client_port;
    char *client_ip;    
    pthread_t thread_id;     // Added to track ID
    bool thread_complete;    // Added for reaping
    SLIST_ENTRY(client_socket_addr_in) entries; // Added for list
};

// 2. Initialize Linked List Head
SLIST_HEAD(slisthead, client_socket_addr_in) head = SLIST_HEAD_INITIALIZER(head);

// create socket filde descriptors
int sfd;
//define thread and mutex
pthread_t time_thread; // client_thread is now handled via the linked list
pthread_mutex_t file_mutex;

volatile sig_atomic_t quit_requested = 0; //quit flag set by signal handler
void signal_handler(int sig)
{
    //Do NOT call printf, syslog, close, unlink, or anything complex inside the handler — unsafe inside signals.
    quit_requested = 1;
    (void)sig; // Silence unused parameter warning
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
}

void shutdown_server_and_clean_up(int cfd)
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

        // 3. Join all threads in the list before exiting
        struct client_socket_addr_in *datap;
        while (!SLIST_EMPTY(&head)) {
            datap = SLIST_FIRST(&head);
            SLIST_REMOVE_HEAD(&head, entries);
            pthread_join(datap->thread_id, NULL);
            free(datap);
        }

        // join all thread
        pthread_join(time_thread, NULL);

        //close and delete /var/tmp/aesdsocketdata. 
        if(unlink(FILE_PATH) == 0){
            printf("deleted %s\n", FILE_PATH);
        }
        else
        {
            perror("unlink error");
        }
        
        pthread_mutex_destroy(&file_mutex);
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
    server_addr.sin_port = htons(PORT); //port 9000 (network byte order)

    if(bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) //bind the socket to the port
    {
        perror("bind failed!");
        close(sfd);
        return -1;
    }
    else printf("Socket bound to port %d \n", ntohs(server_addr.sin_port));

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

      
ssize_t write_to_file(char data_str[], int data_len)
{
    /****** create lock *******/
    pthread_mutex_lock(&file_mutex);

    FILE *fd;
    fd = fopen(FILE_PATH, "a"); //open in append rd wr mode 
    if(fd == NULL)
    {
        perror("failed to open file");
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }
    ssize_t write_s = fwrite(data_str, 1, data_len, fd);
    fflush(fd);// saves data to memory    
    fclose(fd); //close file
    
    /***** release lock *******/
    pthread_mutex_unlock(&file_mutex);

    return write_s;
}


void send_file_to_client(int cfd)
{
    /*send back file content to the client*/

    /****** create lock *******/
    pthread_mutex_lock(&file_mutex);

    char send_buffer[SEND_BUF_SIZE];
    ssize_t no_of_bytes_read; 
    
    FILE *f = fopen(FILE_PATH, "r");
    if(f == NULL)
    {
        perror("failed to open file");
    }
    else
    {
        while((no_of_bytes_read = fread(send_buffer, 1, sizeof(send_buffer), f)) > 0)
        {
            if(send(cfd, send_buffer, no_of_bytes_read, 0) < 0)
            {
                if (errno == EINTR) continue;
                perror("send error");
            }
        }
        fclose(f); //close file
    }
    /***** release lock *******/
    pthread_mutex_unlock(&file_mutex);
}

int process_packets(int cfd, ssize_t bytes_rcv, char recv_buffer[])
{
    //temporal storage for a complete packet
    char packet_buffer[RECV_BUF_SIZE]; //temporal storage for a complete packet

    size_t packet_pos = 0;
    static size_t packet_counter = 0, partial_packet_counter = 0; // static makes it retain valu between calls

    for(ssize_t i = 0; i < bytes_rcv; i++)
    { 
        // /* Protect against available heap buffer overflow */
        if ((packet_pos > RECV_BUF_SIZE) || (bytes_rcv > RECV_BUF_SIZE)) 
        {
            syslog(LOG_ERR, "packet exceeds buffer size, discarding... Packet_pos : %ld\nbytes_rcv : %ld\npacket_size : %ld\n", packet_pos, bytes_rcv, packet_pos);
            printf("packet exceeds buffer size, discarding...\n");
            printf("Packet_pos : %ld\nbytes_rcv : %ld\npacket_size : %ld\n", packet_pos, bytes_rcv, packet_pos);
            packet_pos = 0;
            close(cfd);
            break;
        }

        packet_buffer[packet_pos++] = recv_buffer[i];//extract each packets

        //check for newline
        if (recv_buffer[i] == '\n')
        {
            ssize_t write_s = write_to_file(packet_buffer, packet_pos);

            printf("[Packet %ld] : Size = %ld byte \n%ld byte written to file successfully\n", ((packet_counter++)+1), packet_pos, write_s);//packet_pos = packet size            
            //reset packet buff for next packet
            packet_pos = 0;

            //send back file content to the client
            printf("Sending back file content to client...\n");   
            send_file_to_client(cfd);
        }
        else if(i == (bytes_rcv-1))
        {
            printf("Large packet!!, Handling packets in chunks to prevent buffer overflow...\n");
            ssize_t write_s = write_to_file(packet_buffer, packet_pos);
            packet_pos = 0;
            printf("[%ld]:%ld byte written to file\n\n", ((partial_packet_counter++)+1), write_s);
        }
    }
    return 0;
}

void handle_client(int cfd, char *client_ip, int client_port)
{
    char recv_buffer[RECV_BUF_SIZE]; //
    ssize_t bytes_rcv;
    size_t total_bytes_rcv = 0;

    printf("Recieving packets...\n\n");
    //recieve loop
    while(!quit_requested)
    {
        while ((bytes_rcv = recv(cfd, recv_buffer, sizeof(recv_buffer), 0)) > 0)
        {
            total_bytes_rcv += bytes_rcv;
            printf("--- From %s:%d ----\n", client_ip, client_port);
            printf("Bytes recieved : %zu byte\n", bytes_rcv);
            process_packets(cfd, bytes_rcv, recv_buffer); 
            printf("---Total bytes recieved : %zu byte----\n\n", total_bytes_rcv);
        }
                
        if (bytes_rcv < 0)
        {
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

// 4. Thread function sets the completion flag
void *handle_client_thread(void *p_client_addr)
{
    if (!p_client_addr) return NULL;

    struct client_socket_addr_in *p_client_socket_addr = (struct client_socket_addr_in *)p_client_addr;

    handle_client(p_client_socket_addr->cfd, p_client_socket_addr->client_ip, p_client_socket_addr->client_port);
    close(p_client_socket_addr->cfd);
    
    p_client_socket_addr->thread_complete = true; // Signal main loop to join this thread
    return NULL;
}

//timestamp thread function
void *write_timestamp(void* arg)
{
    (void)arg;
    time_t current_time;
    struct tm time_info;
    char time_str[80];

    while(!quit_requested)
    {
        //get cuurent time
        time(&current_time);
        localtime_r(&current_time, &time_info);
        // Updated to RFC 2822 format as per assignment
        strftime(time_str, sizeof(time_str), "timestamp:%a, %d %b %Y %T %z\n", &time_info);

        //write time str to file
        write_to_file(time_str, strlen(time_str));
        printf("%s", time_str);
        
        // Interrupt-responsive sleep
        for(int i=0; i<10 && !quit_requested; i++) sleep(1);
    }
    return NULL;
}

#define PIDFILE "/var/tmp/aesdsocket.pid" 
void remove_pidfile(void)
{
    unlink(PIDFILE);
}

int daemonize()
{
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
     
    FILE *fp = fopen(PIDFILE, "w");
    if (fp != NULL) {
        fprintf(fp, "%d\n", getpid());
        fclose(fp);
    }
    atexit(remove_pidfile);

    umask(0);
    chdir("/var/tmp");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

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
    
    //init mutex
    pthread_mutex_init(&file_mutex, NULL);

    //open connection to syslog
    openlog("TCP Server(aesdsocket)", LOG_PID|LOG_CONS, LOG_USER );     
    install_signal_handlers();
    
    syslog(LOG_INFO, "Starting TCP server on port %d", PORT);
    if(setup_tcp_server_socket() < 0) quit_requested = true;

    if (daemon_mode == true) daemonize();

    //create client thread
    int rc = pthread_create(&time_thread, NULL, write_timestamp, NULL);
    if (rc != 0){
        syslog(LOG_ERR,"ERROR with timer pthread_create()");
        return false;
    }

    SLIST_INIT(&head); // Initialize the list

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    //accept loop
    while(!quit_requested)
    {
        //accept an incomming connection
        int cfd = accept(sfd, (struct sockaddr *)&client_addr, &client_len);
        if(cfd < 0)
        {
            if(errno == EINTR && quit_requested)
                break; 
            
            perror("\nAccept failed!");
            continue;
        }
        
        struct client_socket_addr_in *p_client_socket_addr = malloc(sizeof(struct client_socket_addr_in));
        p_client_socket_addr->client_ip = inet_ntoa(client_addr.sin_addr);
        p_client_socket_addr->client_port = ntohs(client_addr.sin_port);
        p_client_socket_addr->cfd = cfd;
        p_client_socket_addr->thread_complete = false;
        
        printf("\nAccepting connection from %s:%d\n", p_client_socket_addr->client_ip, p_client_socket_addr->client_port);
        syslog(LOG_INFO, "Accepted connection from %s:%d", p_client_socket_addr->client_ip, p_client_socket_addr->client_port);

        // 5. Create thread and add to list
        if (pthread_create(&p_client_socket_addr->thread_id, NULL, handle_client_thread, p_client_socket_addr) != 0){
            free(p_client_socket_addr);
            close(cfd);
        } else {
            SLIST_INSERT_HEAD(&head, p_client_socket_addr, entries);
        }

        // 6. The Reaper: Join completed threads from the list
        struct client_socket_addr_in *item, *tmp;
        item = SLIST_FIRST(&head);
        while (item != NULL) {
            tmp = SLIST_NEXT(item, entries);
            if (item->thread_complete) {
                pthread_join(item->thread_id, NULL);
                SLIST_REMOVE(&head, item, client_socket_addr_in, entries);
                free(item);
            }
            item = tmp;
        }
    }

    shutdown_server_and_clean_up(-1);
}