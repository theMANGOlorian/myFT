
/*
Da fare:
Inviare comandi al server.
Ricevere cose dal server
Controllo parametri input //ci pensa il server
File remoto non esistente (lettura) //ci pensa il server
Spazio archiviazione insufficiente sul server (scritttura) //ci pensa il server
Interruzione della connessione con il server
controllo file locale eistente?
*/

#include <netinet/in.h> // Struttura per mantenere le informazioni dell'address
#include <stdio.h>
#include <stdlib.h>
#include <string.h>     // Aggiunto per utilizzare strlen
#include <sys/socket.h> // Per creare i socket
#include <sys/types.h>
#include <unistd.h>     // Aggiunto per usare close()
#include <arpa/inet.h> // Aggiunto per usare inet_pton()

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


/*client parsing*/
CommandLineOptions parse_command_line(int argc, char *argv[]) {
    CommandLineOptions options = {0, 0, 0, NULL, 0, NULL, NULL};
    int opt;

    // Parse options
    while ((opt = getopt(argc, argv, "wrl:a:p:f:o:")) != -1) {
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

    // Check required options
    if ((options.w_flag + options.r_flag + options.l_flag) != 1) {
        fprintf(stderr, "Error: You must specify exactly one of -w, -r, or -l\n");
        exit(EXIT_FAILURE);
    }

    if (!options.server_address || !options.port || !options.file_path) {
        fprintf(stderr, "Error: -a, -p, and -f options are required\n");
        exit(EXIT_FAILURE);
    }

    return options;
}

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
int read_file_dimension(const char *path){
    FILE* fp = fopen(path,"rb");
    int size = -1;
    if (fp) {
        fseek(fp,0,SEEK_END);
        size = ftell(fp);
        fclose(fp);
    }
    printf("File size: %d\n",size);
    return size;
}

void download(int socket, const char *remote_name_path, const char *local_name_path) {
    char buffer[BUFFER_SIZE] = {0};
    snprintf(buffer, sizeof(buffer), "GET %d %s", 0,remote_name_path);
    send(socket, buffer, strlen(buffer), 0);    //invio tipo di operazione + path

    FILE *file = fopen(local_name_path, "wb");
    if (file == NULL) {
        perror("File creation failed");
        return;
    }

    int n;
    while ((n = recv(socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, n, file);
    }   
    fclose(file);
    memset(buffer,0,BUFFER_SIZE);
}

void upload(int socket, const char *local_name_path, const char *remote_name_path) {
    char buffer[BUFFER_SIZE] = {0};
    
    // Get the file size for the PUT command
    FILE *file = fopen(local_name_path, "rb");
    if (file == NULL) {
        perror("File not found");
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    snprintf(buffer, sizeof(buffer), "PUT %ld %s\n", file_size, remote_name_path);
    send(socket, buffer, strlen(buffer), 0);  // Send operation type and path

    memset(buffer, 0, BUFFER_SIZE);

    int bytes_sent, bytes_read;

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
}


int main(int argc, char *argv[]) {

    //parsing del comando
    CommandLineOptions options = parse_command_line(argc,argv);
    print_options(&options);


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
    }
    close(sock);

    return 0;
}
