#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT 9000
#define LISTEN_BACKLOG 10
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define RECV_BUF_SZ 1024
#define PACKET_BUF_SZ 4096
#define SEND_BUF_SZ 1024

/* Global state for signal handler */
static volatile sig_atomic_t exit_requested = 0;
static int listen_fd = -1;     // listening socket fd
static int client_fd = -1;     // active client fd (or -1)

/* Signal handler: set flag and log in main context */
static void signal_handler(int signum)
{
    (void)signum;
    exit_requested = 1;
}

/* Install signal handlers for SIGINT and SIGTERM */
static int install_signal_handlers(void)
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // do not set SA_RESTART so accept() can be interrupted

    if (sigaction(SIGINT, &sa, NULL) == -1) return -1;
    if (sigaction(SIGTERM, &sa, NULL) == -1) return -1;
    return 0;
}

/* Open, bind, listen: returns listening socket fd or -1 on failure */
static int start_listen_socket(void)
{
    int sfd;
    struct sockaddr_in serv_addr;
    int opt = 1;

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        syslog(LOG_ERR, "socket() failed: %s", strerror(errno));
        return -1;
    }

    /* Allow immediate reuse of the address */
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        syslog(LOG_ERR, "setsockopt(SO_REUSEADDR) failed: %s", strerror(errno));
        close(sfd);
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    if (bind(sfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        syslog(LOG_ERR, "bind() failed: %s", strerror(errno));
        close(sfd);
        return -1;
    }

    if (listen(sfd, LISTEN_BACKLOG) == -1) {
        syslog(LOG_ERR, "listen() failed: %s", strerror(errno));
        close(sfd);
        return -1;
    }

    return sfd;
}

/* Send entire file contents back to client_fd (streams in chunks). */
static int send_file_to_client(int cfd)
{
    int fd = open(DATA_FILE, O_RDONLY);
    if (fd == -1) {
        /* If file doesn't exist, nothing to send (not necessarily an error). */
        if (errno == ENOENT) return 0;
        syslog(LOG_ERR, "open(%s) for reading failed: %s", DATA_FILE, strerror(errno));
        return -1;
    }

    char buf[SEND_BUF_SZ];
    ssize_t r;
    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t off = 0;
        while (off < r) {
            ssize_t s = send(cfd, buf + off, r - off, 0);
            if (s == -1) {
                syslog(LOG_ERR, "send() failed: %s", strerror(errno));
                close(fd);
                return -1;
            }
            off += s;
        }
    }
    if (r == -1) syslog(LOG_ERR, "read() failed while sending file: %s", strerror(errno));
    close(fd);
    return (r == -1) ? -1 : 0;
}

/* Process a single connected client: receive, detect newline-terminated packets,
   append each packet to DATA_FILE, and after each packet send the whole DATA_FILE back.
   Honours exit_requested flag and will finish the current in-flight work then return. */
static void handle_client(int cfd, const char *client_ip)
{
    char recv_buf[RECV_BUF_SZ];
    char packet_buf[PACKET_BUF_SZ];
    size_t packet_pos = 0;
    ssize_t bytes;

    while (!exit_requested && (bytes = recv(cfd, recv_buf, sizeof(recv_buf), 0)) > 0) {
        for (ssize_t i = 0; i < bytes; ++i) {
            packet_buf[packet_pos++] = recv_buf[i];

            if (recv_buf[i] == '\n') {
                /* Append the packet to the file (open/append/close per-packet) */
                int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd == -1) {
                    syslog(LOG_ERR, "open(%s) for append failed: %s", DATA_FILE, strerror(errno));
                    packet_pos = 0;
                    continue; /* try to continue processing */
                }

                ssize_t wrote = 0;
                while (wrote < (ssize_t)packet_pos) {
                    ssize_t w = write(fd, packet_buf + wrote, packet_pos - wrote);
                    if (w == -1) {
                        if (errno == EINTR) continue;
                        syslog(LOG_ERR, "write() failed: %s", strerror(errno));
                        break;
                    }
                    wrote += w;
                }
                /* Ensure contents are on disk (assignment expects persistence) */
                if (fsync(fd) == -1) {
                    syslog(LOG_ERR, "fsync() failed: %s", strerror(errno));
                }
                close(fd);

                /* After appending, send full file contents back to client */
                if (send_file_to_client(cfd) == -1) {
                    /* send error: break connection */
                    packet_pos = 0;
                    break;
                }

                /* reset packet buffer */
                packet_pos = 0;
                if (packet_pos < PACKET_BUF_SZ) memset(packet_buf, 0, packet_pos); /* harmless */
            }

            /* Protect against packet buffer overflow */
            if (packet_pos >= PACKET_BUF_SZ) {
                syslog(LOG_ERR, "packet exceeds buffer size, discarding");
                packet_pos = 0;
            }
        }
    }

    if (bytes == -1 && !exit_requested) {
        syslog(LOG_ERR, "recv() failed from %s: %s", client_ip, strerror(errno));
    } else if (bytes == 0) {
        /* client closed connection */
    }
    char buf[SEND_BUF_SZ];
    ssize_t r;
    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t off = 0;
        while (off < r) {
}

/* Daemonize: standard fork + setsid + close std fds.
   Per spec, bind/listen must be done before forking; so caller should do that. */
static int daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork() failed: %s", strerror(errno));
        return -1;
    }
    if (pid > 0) {
        /* parent exits */
        _exit(EXIT_SUCCESS);
    }

    /* child */
    if (setsid() == -1) {
        syslog(LOG_ERR, "setsid() failed: %s", strerror(errno));
        return -1;
    }

    /* Optional: change working directory to / */
    if (chdir("/") == -1) {
        syslog(LOG_WARNING, "chdir(/) failed: %s", strerror(errno));
    }

    /* Redirect stdin/stdout/stderr to /dev/null */
    int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int sfd = -1;
    int optdaemon = 0;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN];

    /* parse -d */
    if (argc > 1 && strcmp(argv[1], "-d") == 0) optdaemon = 1;

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting TCP server on port %d", PORT);

    if (install_signal_handlers() == -1) {
        syslog(LOG_ERR, "Failed to install signal handlers");
        closelog();
        return -1;
    }

    /* create, bind, listen (must succeed or return -1) */
    sfd = start_listen_socket();
    if (sfd == -1) {
        closelog();
        return -1;
    }
    listen_fd = sfd; /* global for cleanup */

    /* Daemonize AFTER bind/listen if requested */
    if (optdaemon) {
        if (daemonize() == -1) {
            close(sfd);
            closelog();
            return -1;
        }
        /* Child is now daemon; continue running */
    }

    /* Main accept loop */
    while (!exit_requested) {
        client_fd = -1;
        client_len = sizeof(client_addr);
        client_fd = accept(sfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            if (exit_requested) break; /* we'll exit gracefully */
            if (errno == EINTR) continue; /* interrupted by signal, check exit_requested next loop */
            syslog(LOG_ERR, "accept() failed: %s", strerror(errno));
            continue;
    char buf[SEND_BUF_SZ];
    ssize_t r;
    while ((r = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t off = 0;
        while (off < r) {
        }

        /* Format client IP */
        if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)) == NULL)
            strncpy(client_ip, "unknown", sizeof(client_ip));

        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        /* handle client (will return on client close or error or exit_requested) */
        handle_client(client_fd, client_ip);

        /* close client and log closed message */
        close(client_fd);
        client_fd = -1;
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    /* Exit: log and cleanup */
    syslog(LOG_INFO, "Caught signal, exiting");

    if (client_fd != -1) {
        close(client_fd);
        client_fd = -1;
    }
    if (listen_fd != -1) {
        close(listen_fd);
        listen_fd = -1;
    }

    /* Remove the data file */
    if (unlink(DATA_FILE) == -1) {
        if (errno != ENOENT)
            syslog(LOG_ERR, "Failed to remove %s: %s", DATA_FILE, strerror(errno));
    }

    closelog();
    return 0;
}