/*
Progetto: MyFT
Autore: Mattia Pandolfi
Descrizione: Applicazione Client/Server per il trasferimento file.
*/

/** Esempio di input
 * 
 * ./server -a 127.0.0.1 -p 6969 -d /Scrivania/C/myFT/server_root/
 * 
 * **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/statvfs.h>

#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024

typedef struct {
    char *server_address;
    char *server_port;
    char *ft_root_directory;
} ServerConfig;

typedef struct {
    int client_socket;
    ServerConfig *config;
} ClientHandlerParam;

void acquire_lock_writer(int f){
    struct flock lock;
    lock.l_type = F_WRLCK; // Lock in scrittura
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(f, F_SETLKW, &lock); // Lock e attesa
}
void acquire_lock_reader(int f){
    struct flock lock;
    lock.l_type = F_RDLCK; // Lock in lettura
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

int check_space(unsigned long long fileSize) {
    struct statvfs stat;
    const char *path = "/"; // Usare la radice del filesystem

    // Ottieni le informazioni del filesystem
    if (statvfs(path, &stat) != 0) {
        // Errore nel recupero delle informazioni
        perror("[-] Error:statvfs");
        return 0;
    }

    // Calcola lo spazio libero in bytes
    unsigned long long freeSpace = stat.f_bsize * stat.f_bavail;
    if (freeSpace > fileSize) {
        return 0; // Abbastanza spazio
    } else {
        return 1; // Non abbastanza spazio
    }
}

char* execute_ls_la(const char* path) {
    int pipefd[2];
    pid_t pid;
    char buffer[BUFFER_SIZE];
    char* result = NULL;
    size_t result_len = 0;
    // Creare la pipe
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return NULL;
    }
    // Creare il processo figlio
    pid = fork();
    if (pid == -1) {
        perror("fork");
        return NULL;
    }
    if (pid == 0) { // Processo figlio
        // Chiudere il lato di lettura della pipe
        close(pipefd[0]);
        // Reindirizzare stdout alla pipe
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        // Eseguire il comando "ls -la"
        execlp("ls", "ls", "-la", path, (char *)NULL);
        // Se execlp fallisce
        perror("execlp");
        exit(EXIT_FAILURE);
    } else { // Processo padre
        // Chiudere il lato di scrittura della pipe
        close(pipefd[1]);
        // Leggere l'output del comando dalla pipe
        ssize_t count;
        while ((count = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            result = realloc(result, result_len + count + 1);
            if (result == NULL) {
                perror("realloc");
                close(pipefd[0]);
                return NULL;
            }
            memcpy(result + result_len, buffer, count);
            result_len += count;
        }
        close(pipefd[0]);
        if (count == -1) {
            perror("read");
            free(result);
            return NULL;
        }
        // Null-terminare la stringa risultante
        result[result_len] = '\0';
        // Attendere la terminazione del processo figlio
        wait(NULL);
    }
    return result;
}


void *client_handler(void *param) {

    ClientHandlerParam *handler_param = (ClientHandlerParam *)param;
    int sock = handler_param->client_socket;
    ServerConfig *config = handler_param->config;

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

    char *fullpath = malloc(strlen(config->ft_root_directory) + strlen(path) + 1);
    strcpy(fullpath, config->ft_root_directory);
    strcat(fullpath,path);

    if (strcmp(op,"PUT") == 0){
        // operazione PUT : scrittura di un file ricevuto

        //fare il controllo dello spazio
        if (check_space(fileSize)) {
            perror("[-] Spazio non sufficiente\n");
            char* msg_error = "[-] Not enough space";
            send(sock,msg_error,strlen(msg_error),0);
            return NULL;
        }

        // Apertura del file in scrittura
        FILE *file = fopen(fullpath,"wb");
        if (file == NULL) {
            perror("[-] Errore nell'apertura del file\n");
            char* msg_error = "[-] Error opening the file";
            send(sock,msg_error,strlen(msg_error),0);
            return NULL;
        }

        // conferma positiva
        send(sock,"OK",strlen("OK"),0);

        int fd = fileno(file);
        // Locking del file
        acquire_lock_writer(fd);
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
            char* msg = "File uploaded successfully";
            send(sock,msg, strlen(msg),0);
        }

    } else if (strcmp(op,"GET") == 0){
        // operazione GET : invio di un file

        FILE *file = fopen(fullpath,"rb");
        if (file == NULL) {
            perror("[-] errore nell'apertura del file\n");
            send(sock,"NO",strlen("NO"),0);
            return NULL;
        }

        send(sock,"OK",strlen("OK"),0);
        acquire_lock_reader(fileno(file));

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
        release_lock(fileno(file));
        memset(buffer,0,BUFFER_SIZE);
        

    } else if (strcmp(op,"INF") == 0){
        // operazione INF : lettura directory
        char* output_ls = execute_ls_la(fullpath);
        if (output_ls == NULL){
            perror("[-] Error:output_ls is NULL\n");
            send(sock,"NO",strlen("NO"),0);
            return NULL;
        }
        send(sock,"OK",strlen("OK"),0);
        int bytes_sent = send(sock,output_ls,strlen(output_ls),0);
        if (bytes_sent < 0){
            perror("[-] Errore nell'invio dei dati tramite socket");
            free(output_ls);
            return NULL;
        }
        free(output_ls);
    }

    printf("Client disconnected\n");
    close(sock);
    free(fullpath);
    free(handler_param);
    return NULL;
}


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
                char *user_path = getenv("HOME");
                char *full_path = malloc(strlen(user_path) + strlen(optarg) + 1);
                strcpy(full_path,user_path);
                strcat(full_path,optarg);
                config->ft_root_directory = full_path;
                printf("ft_root_directory: %s\n",config->ft_root_directory);
                //config->ft_root_directory = optarg;
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


    int server_sock, client_sock;
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

        ClientHandlerParam *handler_param = malloc(sizeof(ClientHandlerParam));
        if (handler_param == NULL) {
            perror("Could not allocate memory for handler_args");
            return 1;
        }
        handler_param->client_socket = client_sock;
        handler_param->config = &config;

        if (pthread_create(&client_thread, NULL, client_handler, (void*)handler_param) < 0) {
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

