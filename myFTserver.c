#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024

void acquire_lock(int f){
    struct flock lock;
    lock.l_type = F_WRLCK; // Lock in scrittura
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(f, F_SETLKW, &lock); // Lock e attesa
}

void release_lock(int f){
    struct flock lock;
    lock.l_type = F_UNLCK; // Rilascia il lock
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(f, F_SETLKW, &lock); // Lock e attesa
}


void *client_handler(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[BUFFER_SIZE];
    int bytes_recv = 0;
    char op[10];
    int fileSize;
    char *path;
    char *headerEnd;

    bytes_recv = recv(sock, buffer, BUFFER_SIZE,0);
    if (bytes_recv <= 0){
        perror("[-] Errore nella ricezione dell'header\n ");
        return NULL;
    }

    headerEnd = strstr(buffer,"\n");
    if (headerEnd == NULL) {
        perror("Header non valido\n");
        return NULL;
    }

    int headerSize = headerEnd - buffer+1;
    path = malloc((bytes_recv - headerSize + 1) * sizeof(char));
    sscanf(buffer,"%s %d %s\n", op, &fileSize, path);

    FILE *file = fopen(path,"wb");
    if (file == NULL) {
        perror("[-] Errore nell'apertura del file\n");
        return NULL;
    }

    fwrite(headerEnd+1,1, bytes_recv - headerSize, file);
    fileSize -= bytes_recv - headerSize;
    while (fileSize > 0) {
        bytes_recv = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_recv <= 0){
            perror("[-] Errore nella ricezione dei dati\n");
            break;
        }

        fwrite(buffer,1,bytes_recv,file);
        memset(buffer, 0, BUFFER_SIZE);
        fileSize -= bytes_recv;
    }
    fclose(file);
    printf("Client disconnected\n");

    close(sock);
    free(socket_desc);
    return NULL;
}


int main() {
    int server_sock, client_sock, *new_sock;
    struct sockaddr_in server, client;
    socklen_t client_len = sizeof(struct sockaddr_in);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Could not create socket");
        return 1;
    }
    printf("Socket created\n");

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        return 1;
    }
    printf("Bind done\n");

    listen(server_sock, MAX_CLIENTS);
    printf("Waiting for incoming connections...\n");

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client, &client_len))) {
        printf("Connection accepted\n");

        pthread_t client_thread;
        new_sock = malloc(1);
        *new_sock = client_sock;

        if (pthread_create(&client_thread, NULL, client_handler, (void*)new_sock) < 0) {
            perror("Could not create thread");
            return 1;
        }

        printf("Handler assigned\n");
    }

    if (client_sock < 0) {
        perror("Accept failed");
        return 1;
    }

    close(server_sock);

    return 0;
}

