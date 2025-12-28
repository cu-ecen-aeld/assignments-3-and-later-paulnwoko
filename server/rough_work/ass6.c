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
#include <sys/queue.h> // Required for thread management

#define PORT 9000
#define LISTEN_BACKLOG 50
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define RECV_BUF_SIZE 2048 
#define SEND_BUF_SIZE 2048 

// Thread tracking structure for SLIST
struct thread_data_t {
    pthread_t thread_id;
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
    int client_port;
    bool thread_complete;
    SLIST_ENTRY(thread_data_t) entries;
};

// Original Global Variables
int sfd = -1, cfd = -1;
pthread_t time_thread;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t quit_requested = 0;

// SLIST Head
SLIST_HEAD(slisthead, thread_data_t) head = SLIST_HEAD_INITIALIZER(head);

void signal_handler(int sig)
{
    (void)sig; 
    quit_requested = 1;
}

void install_signal_handlers()
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

void shutdown_server_and_clean_up()
{
    syslog(LOG_INFO, "Caught signal, exiting");
    
    if(sfd != -1) {
        shutdown(sfd, SHUT_RDWR);
        close(sfd);
    }

    // Join all threads in the list
    struct thread_data_t *item;
    while (!SLIST_EMPTY(&head)) {
        item = SLIST_FIRST(&head);
        SLIST_REMOVE_HEAD(&head, entries);
        pthread_join(item->thread_id, NULL);
        free(item);
    }

    pthread_join(time_thread, NULL);

    if(unlink(FILE_PATH) == 0){
        // Only print if not in daemon mode or log to syslog
    }
    
    pthread_mutex_destroy(&mutex);
    closelog();
    exit(0);
}

int setup_tcp_server_socket()
{
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sfd == -1) return -1;

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if(bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        close(sfd);
        return -1;
    }

    if(listen(sfd, LISTEN_BACKLOG) == -1)
    {
        close(sfd);
        return -1;
    }
    return 0;
}

ssize_t write_to_file(FILE *fd, char data_str[], int data_len)
{
    pthread_mutex_lock(&mutex);
    // Re-open to ensure append pointer is correct in multithreaded environment
    FILE *fp = fopen(FILE_PATH, "a"); 
    if(!fp) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    ssize_t write_s = fwrite(data_str, 1, data_len, fp);
    fclose(fp);
    pthread_mutex_unlock(&mutex);
    (void)fd; // Original signature used fd, but multithreading requires opening/closing inside lock
    return write_s;
}

int process_packets(int client_fd, ssize_t bytes_rcv, char recv_buffer[])
{
    char packet_buffer[RECV_BUF_SIZE];
    size_t packet_pos = 0;

    for(ssize_t i = 0; i < bytes_rcv; i++)
    { 
        packet_buffer[packet_pos++] = recv_buffer[i];

        if (recv_buffer[i] == '\n')
        {
            write_to_file(NULL, packet_buffer, packet_pos);
            packet_pos = 0;

            // Send back file content
            pthread_mutex_lock(&mutex);
            FILE *f_read = fopen(FILE_PATH, "r");
            if(f_read) {
                char send_buffer[SEND_BUF_SIZE];
                ssize_t n_read;
                while((n_read = fread(send_buffer, 1, sizeof(send_buffer), f_read)) > 0)
                {
                    send(client_fd, send_buffer, n_read, 0);
                }
                fclose(f_read);
            }
            pthread_mutex_unlock(&mutex);
        }
    }
    return 0;
}

void handle_client(int client_cfd, char *client_ip, int client_port)
{
    char recv_buffer[RECV_BUF_SIZE];
    ssize_t bytes_rcv;

    while (!quit_requested) {
        bytes_rcv = recv(client_cfd, recv_buffer, sizeof(recv_buffer), 0);
        if (bytes_rcv > 0) {
            process_packets(client_cfd, bytes_rcv, recv_buffer);
        } else {
            break; 
        }
    }
    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    (void)client_port;
}

void *handle_client_thread(void *p_client_addr)
{
    if (!p_client_addr) return NULL;
    struct thread_data_t *data = (struct thread_data_t *)p_client_addr;

    handle_client(data->client_fd, data->client_ip, data->client_port);
    
    close(data->client_fd);
    data->thread_complete = true;
    return NULL;
}

void *write_timestamp(void* arg)
{
    (void)arg;
    while(!quit_requested)
    {
        for (int i = 0; i < 10 && !quit_requested; i++) sleep(1);
        if (quit_requested) break;

        time_t rawtime;
        struct tm *info;
        char buffer[100];
        time(&rawtime);
        info = localtime(&rawtime);
        // RFC 2822 format
        int len = strftime(buffer, sizeof(buffer), "timestamp:%a, %d %b %Y %T %z\n", info);

        write_to_file(NULL, buffer, len);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    bool daemon_mode = (argc == 2 && strcmp(argv[1], "-d") == 0);
    
    openlog("aesdsocket", LOG_PID, LOG_USER);
    install_signal_handlers();
    
    if(setup_tcp_server_socket() < 0) exit(-1);

    if (daemon_mode) {
        if (daemon(0, 0) == -1) exit(-1);
    }

    SLIST_INIT(&head);
    pthread_create(&time_thread, NULL, write_timestamp, NULL);

    while(!quit_requested)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int current_cfd = accept(sfd, (struct sockaddr *)&client_addr, &client_len);
        
        if(current_cfd < 0) {
            if (quit_requested) break;
            continue;
        }

        struct thread_data_t *new_thread = malloc(sizeof(struct thread_data_t));
        new_thread->client_fd = current_cfd;
        new_thread->thread_complete = false;
        new_thread->client_port = ntohs(client_addr.sin_port);
        inet_ntop(AF_INET, &client_addr.sin_addr, new_thread->client_ip, sizeof(new_thread->client_ip));

        syslog(LOG_INFO, "Accepted connection from %s", new_thread->client_ip);

        if (pthread_create(&new_thread->thread_id, NULL, handle_client_thread, new_thread) != 0) {
            close(current_cfd);
            free(new_thread);
        } else {
            SLIST_INSERT_HEAD(&head, new_thread, entries);
        }

        // Join completed threads
        struct thread_data_t *item, *tmp;
        item = SLIST_FIRST(&head);
        while (item != NULL) {
            tmp = SLIST_NEXT(item, entries);
            if (item->thread_complete) {
                pthread_join(item->thread_id, NULL);
                SLIST_REMOVE(&head, item, thread_data_t, entries);
                free(item);
            }
            item = tmp;
        }
    }

    shutdown_server_and_clean_up();
    return 0;
}





// Continuation of Assignment 5:

// Modify your socket based program to accept multiple simultaneous connections, with each connection spawning a new thread to handle the connection.

//         a. Writes to /var/tmp/aesdsocketdata should be synchronized between threads using a mutex, to ensure data written by synchronous connections is not intermixed, and not relying on any file system synchronization.

//                 i. For instance, if one connection writes “12345678” and another connection writes “abcdefg” it should not be possible for the resulting /var/tmp/aesdsocketdata file to contain a mix like “123abcdefg456”, the content should always be “12345678”, followed by “abcdefg”.  However for any simultaneous connections, it's acceptable to allow packet writes to occur in any order in the socketdata file.

//         b. The thread should exit when the connection is closed by the client or when an error occurs in the send or receive steps.

//         c. Your program should continue to gracefully exit when SIGTERM/SIGINT is received, after requesting an exit from each thread and waiting for threads to complete execution.

//         d. Use the singly linked list APIs discussed in the video (or your own implementation if you prefer) to manage threads.

//                 i. Use pthread_join() to join completed threads, do not use detached threads for this assignment.

// 2. Modify your aesdsocket source code repository to:

//         a. Append a timestamp in the form “timestamp:time” where time is specified by the RFC 2822 compliant strftime format



// , followed by newline.  This string should be appended to the /var/tmp/aesdsocketdata file every 10 seconds, where the string includes the year, month, day, hour (in 24 hour format) minute and second representing the system wall clock time.

//         b. Use appropriate locking to ensure the timestamp is written atomically with respect to socket data

//         Hint: 

// 	        Think where should the timer be initialized. Should it be initialized in parent or child?

// 3. Use the updated sockettest.sh script (in the assignment-autotest/test/assignment6 subdirectory) . You can run this manually outside the `./full-test.sh` script by:

//         a. Starting your aesdsocket application

//         b. Executing the sockettest.sh script from the assignment-autotest subdirectory.

//         c. Stopping your aesdsocket application.

// i will send you the assignment 5 code so u modify it accordingly