#ifndef MYFTSERVER_H
#define MYFTSERVER_H

#define MAX_CLIENTS 100
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

/* Funzioni per il lock dei file */
void acquire_lock_writer(int f);
void acquire_lock_reader(int f);
void release_lock(int f);

/* Funzione per controllare lo spazio disponibile */
int check_space(unsigned long long fileSize);

/* Funzione per eseguire il comando "ls -la" e restituire l'output */
char* execute_ls_la(const char* path);

/* Funzione per creare la directory server root */
int make_server_root_dir(const char *path);

/* Funzione per gestire la connessione del client */
void *client_handler(void *param);

/* Funzione per il parsing degli argomenti della linea di comando */
int parse_arguments(int argc, char *argv[], ServerConfig *config);

void sigchld_handler(int sig) ;

#endif