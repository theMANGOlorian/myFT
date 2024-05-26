#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024
#define PORT 8080

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
    int bytes_recv;
    char *op;
    int fileSize;
    char *path;

    // Ricezione della richiesta
    bytes_recv = recv(sock, buffer, BUFFER_SIZE,0);
    if (bytes_recv <= 0){
        perror("[-] Errore nella ricezione della richiesta\n ");
        return NULL;
    }
    op = strtok(buffer," ");
    fileSize = atoi(strtok(NULL," "));
    path = strtok(NULL,"\n");
    printf("op: %s\npath: %s\nfileSize: %d\n",op, path, fileSize);

    if (strcmp(op,"PUT") == 0){
        // operazione PUT : scrittura di un file ricevuto

        //fare il controllo dello spazio
        if (0) {
            send(sock,"Error",strlen("Error"),0);
            return NULL;
        }

        // Apertura del file in scrittura
        FILE *file = fopen(path,"wb");
        if (file == NULL) {
            perror("[-] Errore nell'apertura del file\n");
            send(sock,"Error",strlen("Error"),0);
            return NULL;
        }

        // conferma positiva
        send(sock,"OK",strlen("OK"),0);

        int fd = fileno(file);
        // Locking del file
        acquire_lock(fd);
        int error = 0;
        while (fileSize > 0) {
            bytes_recv = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes_recv <= 0){
                perror("[-] Errore nella ricezione dei dati\n");
                error = 1;
                break;
            }

            fwrite(buffer,1,bytes_recv,file);
            memset(buffer, 0, BUFFER_SIZE);
            fileSize -= bytes_recv;
        }
        fclose(file);
        // Unlocking del file
        release_lock(fd);

        if (!error){
            send(sock,"File caricato con successo", strlen("File caricato con successo"),0);
        }

    } else if (strcmp(op,"GET") == 0){
        // operazione GET : invio di un file

        FILE *file = fopen(path,"rb");
        if (file == NULL) {
            perror("[-] errore nell'apertura del file\n");
            send(sock,"NO",strlen("NO"),0);
            return NULL;
        }

        send(sock,"OK",strlen("OK"),0);
        int bytes_read, bytes_sent;
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            bytes_sent = send(sock, buffer, bytes_read, 0);
            if (bytes_sent < 0) {
                perror("[-] Errore nell'invio dei dati tramite socket");
                fclose(file);
                return NULL;
            }
        }
        fclose(file);
        memset(buffer,0,BUFFER_SIZE);
        

    } else {
        // operazione INF : lettura directory

    }

    printf("Client disconnected\n");
    close(sock);
    free(socket_desc);
    return NULL;
}

typedef struct {
    char *server_address;
    char *server_port;
    char *ft_root_directory;
} ServerConfig;

int parse_arguments(int argc, char *argv[], ServerConfig *config) {
    int opt;

    // Initialize the config structure with NULL values
    config->server_address = NULL;
    config->server_port = NULL;
    config->ft_root_directory = NULL;

    // Parse command line arguments using getopt
    while ((opt = getopt(argc, argv, "a:p:d:")) != -1) {
        switch (opt) {
            case 'a':
                config->server_address = optarg;
                break;
            case 'p':
                config->server_port = optarg;
                break;
            case 'd':
                config->ft_root_directory = optarg;
                break;
            default:
                printf("Usage: -a server_address -p server_port -d ft_root_directory\n");
                return 1;
        }
    }

    // Check if all required parameters are provided
    if (config->server_address == NULL || config->server_port == NULL || config->ft_root_directory == NULL) {
        printf("Usage: -a server_address -p server_port -d ft_root_directory\n");
        return 1;
    }

    return 0;
}


int main(int argc, char *argv[]) {

    ServerConfig config;
    // gestione argomenti linea di comando
    if (parse_arguments(argc, argv, &config) != 0) {
        return 1;
    }


    int server_sock, client_sock, *new_sock;
    struct sockaddr_in server, client;
    socklen_t client_len = sizeof(struct sockaddr_in);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Could not create socket");
        return 1;
    }
    printf("Socket created\n");

    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(config.server_address);
    server.sin_port = htons(atoi(config.server_port));

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

