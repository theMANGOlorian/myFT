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
    char *path = "server_root/text.txt", *op = "PUT";
    //int sizeIncoming;
    //char header[BUFFER_SIZE];

    /*
    recv(sock, header, BUFFER_SIZE, 0);
    */
    /*riceve le informazioni iniziali, tipo di operazione, numero di bytes e il path*/
    /*
    char *token = strtok(header, " ");
    if (token == NULL) {
        printf("Comando non valido\n");
        close(sock);
        return NULL;
    }
    op = strdup(token);
    token = strtok(NULL," ");
    if (token == NULL){
        printf("Grandezza non valida\n");
        close(sock);
        return NULL;
    }
    sizeIncoming = atoi(token);
    token = strtok(NULL, "\n");
    if (token == NULL) {
        printf("Percorso non valido\n");
        close(sock);
        return NULL;
    }
    path = strdup(token);

    memset(buffer,0,sizeof(buffer));

    printf("size file: %d\n", sizeIncoming);
    printf("operation : %s\n", op);
    printf("path : %s\n", path);

    */
    if (strcmp(op, "GET") == 0) {
        /** Reading operation **/


    } else if (strcmp(op, "PUT") == 0) {
        /** Writing operation **/

        FILE *f = fopen(path, "w");

        if (f == NULL) {
            perror("[-] Errore nell'apertura del file\n");
            exit(1);
        }

        int fd = fileno(f); //restituisce il descrittore del file associato a quel FILE

        acquire_lock(fd);
        // Dopo l'acquisizione del lock
        
        int n;
        while (1) {
            n = recv(sock, buffer, BUFFER_SIZE,0);
            if (n <= 0) {
                break;
            }
        }

        // Dopo il rilascio del lock
        release_lock(fd);

        fclose(f);
    }

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

