#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000
#define BUFSIZE 4096

int main() {
    int sfd, cfd;
    struct sockaddr_in addr;
    char buffer[BUFSIZE];

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
    listen(sfd, 1);

    printf("Listening on port %d...\n", PORT);
    cfd = accept(sfd, NULL, NULL);
    if (cfd < 0) { perror("accept"); exit(1); }

    ssize_t bytes;
    size_t total = 0;
    while ((bytes = recv(cfd, buffer, sizeof(buffer), 0)) > 0) {
        printf("recv() got %zd bytes\n", bytes);
        total += bytes;
    }
    printf("Total bytes received: %zu\n", total);

    close(cfd);
    close(sfd);
    return 0;
}
