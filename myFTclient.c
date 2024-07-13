
/*
Progetto: MyFT
Autore: Mattia Pandolfi
Descrizione: Applicazione Client/Server per il trasferimento file.
*/
/**
 * ./client -w -a 127.0.0.1 -p 6969 -f /Scrivania/C/myFT/client_dir/Ring.txt -o Ring.txt
 * ./client -l -a 127.0.0.1 -p 6969 -f .
 * ./client -r -a 127.0.0.1 -p 6969 -f Ring.txt -o /Scrivania/C/myFT/client_dir/Ring.txt
 * 
 * **/

#include <netinet/in.h> // Struttura per mantenere le informazioni dell'address
#include <stdio.h>
#include <stdlib.h>
#include <string.h>     // Aggiunto per utilizzare strlen
#include <sys/socket.h> // Per creare i socket
#include <sys/types.h>
#include <unistd.h>     // Aggiunto per usare close()
#include <arpa/inet.h> // Aggiunto per usare inet_pton()

#include "myFTclient.h"

/*client parsing*/
CommandLineOptions parse_command_line(int argc, char *argv[]) {
    CommandLineOptions options = {0, 0, 0, NULL, 0, NULL, NULL};
    int opt;

    // Parse delle opzione selezionate
    while ((opt = getopt(argc, argv, "wrl::a:p:f:o:")) != -1) {
        switch (opt) {
            case 'w':
                if (options.r_flag || options.l_flag) {
                    fprintf(stderr, "Error: -w option cannot be used with -r or -l\n");
                    exit(EXIT_FAILURE);
                }
                options.w_flag = 1;
                break;
            case 'r':
                if (options.w_flag || options.l_flag) {
                    fprintf(stderr, "Error: -r option cannot be used with -w or -l\n");
                    exit(EXIT_FAILURE);
                }
                options.r_flag = 1;
                break;
            case 'l':
                if (options.w_flag || options.r_flag) {
                    fprintf(stderr, "Error: -l option cannot be used with -w or -r\n");
                    exit(EXIT_FAILURE);
                }
                options.l_flag = 1;
                break;
            case 'a':
                options.server_address = optarg;
                break;
            case 'p':
                options.port = atoi(optarg); //atoi() converto da char a int
                break;
            case 'f':
                options.file_path = optarg;
                break;
            case 'o':
                options.output_path = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -[w|r|l] -a server_address -p port -f path [-o output_path]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // controllo delle opzione necessarie
    if ((options.w_flag + options.r_flag + options.l_flag) != 1) {
        fprintf(stderr, "Error: You must specify exactly one of -w, -r, or -l\n");
        exit(EXIT_FAILURE);
    }

    if (!options.server_address || !options.port || !options.file_path) {
        fprintf(stderr, "Error: -a, -p, and -f options are required\n");
        exit(EXIT_FAILURE);
    }

    if (!options.output_path){
        options.output_path = options.file_path;
    }

    return options;

}
// stampa informazioni sulla richiesta inviata
void print_options(const CommandLineOptions *options) {
    printf("Command Line Options:\n");
    printf("  w_flag: %d\n", options->w_flag);
    printf("  r_flag: %d\n", options->r_flag);
    printf("  l_flag: %d\n", options->l_flag);
    printf("  server_address: %s\n", options->server_address);
    printf("  port: %d\n", options->port);
    printf("  file_path: %s\n", options->file_path);
    if (options->output_path) {
        printf("  output_path: %s\n", options->output_path);
    } else {
        printf("  output_path: (null)\n");
    }
}

/*Funzione per inviare una richiesta di download*/
void download(int socket, const char *remote_name_path, const char *local_name_path) {

    char buffer[BUFFER_SIZE] = {0};
    // costruzione del messaggio di richiesta da inviare al server
    snprintf(buffer, sizeof(buffer), "GET %d %s\n", 0,remote_name_path);
    send(socket, buffer, strlen(buffer), 0);    //invio tipo di operazione + path

    // ottiene il percorso della home directory dell'utente corrente
    char *user_path = getenv("HOME");
    if (user_path == NULL){
        perror("[-] Error:getenv()\n");
        exit(1);
    }

    // percorso completo relativo all'utente che sta eseguendo il programma
    char *full_local_path = malloc(strlen(user_path) + strlen(local_name_path) + 1);
    strcpy(full_local_path,user_path);
    strcat(full_local_path,local_name_path);

    // apre il file locale in modalità scrittura binaria
    FILE *file = fopen(full_local_path, "wb");
    if (file == NULL) {
        perror("File creation failed");
        exit(1);
    }
    // ricezione del primo blocco di dati dal server
    int bytes_recv;
    bytes_recv = recv(socket,buffer,BUFFER_SIZE,0);
    if (bytes_recv <= 0){
        perror("[-] Errore nella ricezione del file\n");
        if (remove(full_local_path) != 0)
            perror("Error deleting file");
        exit(1);
    }
    // controllo della risposta negativa dal server
    if (strncmp(buffer,"[-]",3) == 0){
        printf("Server: %s\n",buffer);
        if (remove(full_local_path) != 0)
            perror("Error deleting file");
        exit(1);
    }
    // rimuove l'header dal buffer prima di scrivere il file
    memmove(buffer,buffer+2, BUFFER_SIZE);
    fwrite(buffer, 1, bytes_recv-2, file); // scrive il primo blocco di dati ricevuto
    // riceve e scrive il resto del file fino a quando non ci sono più dati
    int error = 0;
    while ((bytes_recv = recv(socket, buffer, BUFFER_SIZE, 0)) > 0) {
        if (bytes_recv < 0){
            perror("[-] Errore nella ricezione dei dati\n");
            error = 1;
            break;
        }
        fwrite(buffer, 1, bytes_recv, file);
        memset(buffer,0,BUFFER_SIZE);
    }
    
    fclose(file);

    
    if (!error){
        printf("File downloaded successfully\n");
    }else{
        printf("Error while reading file\n");
        if (remove(full_local_path) != 0)
            perror("Error deleting file");
    }
    free(full_local_path);
    
}

void upload(int socket, const char *local_name_path, const char *remote_name_path) {
    //Caricamento di un file sul server
    
    char buffer[BUFFER_SIZE] = {0};
    int bytes_sent, bytes_read, bytes_recv;

    // ottiene il percorso della home directory dell'utente corrente
    char *user_path = getenv("HOME");
    if (user_path == NULL){
        perror("[-] Error:getenv()\n");
        exit(1);
    }
    // ottiene il percorso completo del file locale da caricare
    char *full_local_path = malloc(strlen(user_path) + strlen(local_name_path) + 1);
    strcpy(full_local_path,user_path);
    strcat(full_local_path,local_name_path);

    // apre il file locale in modalità lettura binaria
    FILE *file = fopen(full_local_path, "rb");
    if (file == NULL) {
        perror("File not found\n");
        exit(1);
    }

    // determina la dimesione del file
    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Error seeking to end of file");
        exit(1);
    }
    long file_size = ftell(file);
    if (file_size == -1L) {
        perror("Error getting file size");
        exit(1);
    }
    fseek(file, 0, SEEK_SET);


    // Icostruisce il messaggio di richiesta di scrittura e lo invia al server
    snprintf(buffer, sizeof(buffer), "PUT %ld %s\n", file_size, remote_name_path);
    send(socket, buffer, strlen(buffer), 0);
    memset(buffer, 0, BUFFER_SIZE);

    // Attesa conferma dal server
    bytes_recv = recv(socket, buffer, BUFFER_SIZE,0);
    if (bytes_recv <= 0){
        perror("[-] Errore nella ricezione della conferma di invio\n");
        exit(1);
    }
    // se il server risponde con un messaggio di errore, termina l'upload
    if (strstr(buffer, "[-]") != NULL){
        printf("Server: %s\n",buffer);
        exit(1);
    }
    
    //invia il file al server in blocchi
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        bytes_sent = send(socket, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("[-] Errore nell'invio dei dati tramite socket");
            fclose(file);
            exit(EXIT_FAILURE);
        }
    }
    memset(buffer, 0, BUFFER_SIZE);
    fclose(file);

    // ricezione esito del salvataggio dal server
    bytes_recv = recv(socket,buffer, BUFFER_SIZE,0);
    if (bytes_recv <= 0){
        perror("[-] Errore nella ricezione dell'esito del salvataggio\n");
        exit(1);
    }
    printf("%s\n",buffer);
    free(full_local_path);
}

// esplorazione di una directory remota sul server
void explore(int socket, const char *remote_name_path){

    char buffer[BUFFER_SIZE] = {0};
    // costruisce il messaggio di richiesta di esplorazione e lo invia al server
    snprintf(buffer, sizeof(buffer), "INF %d %s\n", 0, remote_name_path);
    send(socket, buffer, strlen(buffer), 0);

    // riceve la risposta dal server
    memset(buffer,0,BUFFER_SIZE);
    int bytes_recv;
    bytes_recv = recv(socket,buffer,BUFFER_SIZE,0);
    if (bytes_recv <= 0){
        perror("[-] Errore nella ricezione del output\n");
        exit(1);
    }
    // se il server risponde con un messaggio di errore, termina l'esecuzione
    if (strncmp(buffer,"NO",2) == 0){
        printf("[-] Errore nella ricezione del output : check the file path\n");
        exit(1);
    }
    // sposta il contenuto del buffer di 2 per rimuovere la conferma OK
    memmove(buffer,buffer+2, BUFFER_SIZE);
    printf("%s",buffer);

    // Stampa la parte ricevuta del contenuto della directory fino a quando ci sono dati da ricevere
    while ((bytes_recv = recv(socket, buffer, BUFFER_SIZE, 0)) > 0) {
        printf("%s",buffer);
        memset(buffer,0,BUFFER_SIZE);
    }

}


int main(int argc, char *argv[]) {

    //parsing del comando
    CommandLineOptions options = parse_command_line(argc,argv);

    //print_options(&options); Scommentare per vedere opzioni e parametri messi.


    int sock;
    struct sockaddr_in server_addr;

    // Creazione del socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket fallita\n");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(options.port);

    // Converti indirizzo IP da testo a binario
    if (inet_pton(AF_INET, options.server_address, &server_addr.sin_addr) <= 0) {
        perror("inet_pton error occurred\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    // Connessione al server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connessione fallita\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
    
    if (options.w_flag) {
        // Richiesta di scrittura
        upload(sock,options.file_path,options.output_path);
    } 
    
    else  if (options.r_flag) {
        // Richiesta di lettura
        download(sock,options.file_path,options.output_path);
    }
    
    else if (options.l_flag) {
        // richiesta di infomazioni
        explore(sock,options.file_path);
    }
    close(sock);

    return 0;
}
