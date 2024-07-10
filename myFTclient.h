#ifndef MYFTCLIENT_H
#define MYFTCLIENT_H

#define BUFFER_SIZE 1024

typedef struct {
    int w_flag;
    int r_flag;
    int l_flag;
    char *server_address;
    int port;
    char *file_path;
    char *output_path;
} CommandLineOptions;

/* Funzione di parsing della linea di comando */
CommandLineOptions parse_command_line(int argc, char *argv[]);

/* Funzione per stampare le opzioni della linea di comando */
void print_options(const CommandLineOptions *options);

/* Funzione per il download di un file dal server */
void download(int socket, const char *remote_name_path, const char *local_name_path);

/* Funzione per l'upload di un file sul server */
void upload(int socket, const char *local_name_path, const char *remote_name_path);

/* Funzione per esplorare il contenuto del server */
void explore(int socket, const char *remote_name_path);

#endif