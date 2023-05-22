#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "global.h"
#include "commandes.h"


char ip[16];
int port;
int client_socket;
int upload_socket;
int download_socket;
struct sockaddr_in download_address;

//Nom du fichier lors de l'upload ou du download
char filename[MAX_CHAR];

void * uploadFile() {

    // Création de la socket
    struct sockaddr_in upload_address;
    upload_socket = socket(AF_INET, SOCK_STREAM, 0);
    upload_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(upload_address.sin_addr));
    upload_address.sin_port = htons(port + 1);
    
    //connect au serveur avec descripteur upload
    int uploadConnection = connect(upload_socket, (struct sockaddr*) &upload_address, sizeof(upload_address));
    if (uploadConnection == -1) {
        perror("Erreur lors de la connexion pour upload");
        exit(0);
    }
    
    char filepath[MAX_CHAR] = {0};
    strcat(filepath, CLIENT_TRANSFER_FOLDER);
    strcat(filepath, filename);

    // Assuming 'filename' holds the name of the file to be displayed
    FILE* file = fopen(filepath, "r");

    if (file != NULL) {
        send(upload_socket, filename, strlen(filename) + 1 , 0);
        fseek(file, 0, SEEK_END);
        int size = ftell(file);
        fseek(file, 0, SEEK_SET);
        printf("\033[36m[INFO]\033[0m Taille du fichier : %d\n", size);
        send(upload_socket, &size, sizeof(size), 0);
        char lines[MAX_CHAR] = {0};
        while (fgets(lines, sizeof(lines), file) != NULL) {
            // Envoie le message au serveur
            // int send(int dS, const void *m, size_t lg, int flags)
            // Renvoie le nombre d'octet envoyé si la connexion est réussi et -1 si elle échoue
            // dS = descripteur de socket
            // m = message
            // strlen(m) + 1 = taille du message
            // 0 = protocole par défaut
            send(upload_socket, lines, strlen(lines), 0);
        }
    } 
    else {
        // File does not exist
        printf("\033[41m[ERROR]\033[0m Erreur fichier introuvable\n");
    }

    if (close(upload_socket) == -1) {
        perror("Erreur lors de la fermeture de la socket");
    }

    filename[0] = '\0';

    pthread_exit(0);
}

//TODO
void * downloadFile() {
    
    // Création de la socket
    download_socket = socket(AF_INET, SOCK_STREAM, 0);
    download_address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(download_address.sin_addr));
    download_address.sin_port = htons(port + 2);
    
    //connect au serveur avec descripteur download
    int downloadConnection = connect(download_socket, (struct sockaddr*) &download_address, sizeof(download_address));
    if (downloadConnection == -1) {
        perror("Erreur lors de la connexion pour download");
        exit(0);
    }

    // Envoie le message au serveur
    // int send(int dS, const void *m, size_t lg, int flags)
    // Renvoie le nombre d'octet envoyé si la connexion est réussi et -1 si elle échoue
    // dS = descripteur de socket
    // m = message
    // strlen(m) + 1 = taille du message
    // 0 = protocole par défaut
    send(download_socket, filename, strlen(filename) + 1 , 0);

    char filepath[MAX_CHAR] = {0};
    strcat(filepath, CLIENT_TRANSFER_FOLDER);
    strcat(filepath, filename);

    FILE* file = fopen(filepath, "w");

    if (file != NULL) {
        int size;
        recv(download_socket, &size, sizeof(size), 0);
        printf("\033[36m[INFO]\033[0m Taille du fichier : %d\n", size);
        char lines[MAX_CHAR] = {0};
        while (recv(download_socket, lines, sizeof(lines), 0) > 0) {
            fputs(lines, file);
        }
    } 
    else {
        // File does not exist
        printf("\033[41m[ERROR]\033[0m Erreur fichier introuvable\n");
    }

    if (close(download_socket) == -1) {
        perror("Erreur lors de la fermeture de la socket");
    }

    filename[0] = '\0';

    pthread_exit(0);
}

void * messageReceive() {

    char message[MAX_CHAR] = {0};

    while(1) {

        // Le client attend la réponse du serveur
        int recvC = recv(client_socket, message, sizeof(message), 0);
        if (recvC == -1 || recvC == 0) {
            if(recvC == 0){
                printf("\033[36m[INFO]\033[0m Le serveur a fermé la connexion\n");
            }
            else if(recvC == -1){
                perror("Erreur lors de la réception du message");
            }
            exit(0);
        }
        printf("%s\n", message);
    }
}

void * messageSend() {

    while(1) {
        char message[MAX_CHAR] = {0};
                
        fgets(message, MAX_CHAR, stdin);
        if(message[strlen(message) - 1] == '\n'){
            message[strlen(message) - 1] = '\0';
        }

        if(strlen(message) > MAX_CHAR){
            printf("\033[41m[ERROR]\033[0m Erreur message trop grand\n");
        }
        else{

            //sudo upload
            if (strcmp(message, "sudo upload") == 0) {
                
                //affiche la liste des fichiers qu'il y a dans le dossier spécifié pour le client
                printf("\033[36m[INFO]\033[0m Liste des fichiers dans le dossier spécifié pour le client:\n");
                
                //On affiche la liste des fichiers dans le dossier spécifié
                char command[MAX_CHAR] = "ls ";
                strcat(command, CLIENT_TRANSFER_FOLDER);
                strcat(command, " > list.txt");
                system(command);

                //On ouvre le fichier list.txt
                FILE* file = fopen("list.txt", "r");
                if(file == NULL){
                    perror("Erreur lors de l'ouverture du fichier list.txt");
                    exit(0);
                }

                //On lit le fichier list.txt et on l'envoie au client
                char line[MAX_CHAR];
                while(fgets(line, MAX_CHAR, file) != NULL){

                    //On enlève le \n à la fin de la ligne
                    line[strlen(line) - 1] = '\0';

                    printf("%s\n", line);
                }
                fclose(file);

                //On supprime le fichier list.txt
                if(remove("list.txt") == -1){
                    perror("Erreur lors de la suppression du fichier list.txt");
                    exit(0);
                }

                printf("\033[33m[NEED]\033[0m Veuillez entrer le nom du fichier à envoyer:\n");

                fgets(filename, MAX_CHAR, stdin);
                if(filename[strlen(filename) - 1] == '\n'){
                    filename[strlen(filename) - 1] = '\0';
                }       
                
                pthread_t threadUpload;
                pthread_create(&threadUpload, NULL, uploadFile, NULL);
            }
            else if(strcmp(message, "sudo download") == 0) {
                send(client_socket, message, strlen(message) + 1 , 0);
                fgets(filename, MAX_CHAR, stdin);
                if(filename[strlen(filename) - 1] == '\n'){
                    filename[strlen(filename) - 1] = '\0';
                }

                // Création du thread pour le download
                pthread_t threadDownload;
                pthread_create(&threadDownload, NULL, downloadFile, NULL);
            }
            else {

            // Envoie le message au serveur
            // int send(int dS, const void *m, size_t lg, int flags)
            // Renvoie le nombre d'octet envoyé si la connexion est réussi et -1 si elle échoue
            // dS = descripteur de socket
            // m = message
            // strlen(m) + 1 = taille du message
            // 0 = protocole par défaut
            send(client_socket, message, strlen(message) + 1 , 0);
            }
        }
    }
}

//gestion de signaux
void sigint_handler(int sig) {
    char messageStop[MAX_CHAR] = "sudo quit";
    
    printf("\n\033[36m[INFO]\033[0m Signal CTRL+C reçu, fermeture de la messagerie\n");
    
    //si un signal CTRL+C est reçu, on ferme le client mais pas le serveur
    
    if(send(client_socket, messageStop, strlen(messageStop) + 1 , 0) == -1){
        perror("Erreur lors de l'envoi du message");
    }

    if(close(client_socket) == -1 && close(upload_socket) == -1 && close(download_socket) == -1){
        perror("Erreur lors de la fermeture de la socket");
    }

    exit(0);
}


int main(int argc, char *argv[]) {

    // VERIFICATION DU NOMBRE D'ARGUMENTS
    // argv[0] = client
    // argv[1] = adresse IP du serveur
    // argv[2] = port du serveur
    if (argc != 3) {
        printf("\033[41m[ERROR]\033[0m Erreur nombre d'argument\n");
        printf("\033[36m[INFO]\033[0m Usage: %s <adresse IP> <port>\n", argv[0]);
        exit(0);
    }

    strcpy(ip, argv[1]);
    port = atoi(argv[2]);

    // CREATION DE LA SOCKET DU CLIENT

    // int socket(int domaine, int type, int protocole)
    // Renvoie le descripteur de socket si la création est réussi et -1 si elle échoue
    // PF_INET = Protocole IP
    // SOCK_STREAM = Protocole TCP
    // 0 = Protocole par défaut
    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    // Gestion d'erreur
    if (client_socket == -1) {
        perror("Erreur de création de socket client");
        exit(0);
    }

    // Socket Créé

    // DEFINITION DE L'ADRESSE IP ET DU PORT D'ECOUTE DU SERVEUR

    // Adresse du serveur
    struct sockaddr_in server_address;

    // Stock la famille d'adresse dans la structure aS
    // AF_INET = Protocole IP
    server_address.sin_family = AF_INET;

    // Converti l'adresse IP en format réseau
    // inet_pton(int af, const char *src, void *dst)
    // Renvoie 1 si la conversion est réussi et 0 si elle échoue
    // INADDR_ANY = adresse IP de la machine
    // argv[1] = adresse IP du serveur
    // server_adress.sin_addr = adresse du serveur
    inet_pton(AF_INET,argv[1],&(server_address.sin_addr));

    // Converti le port en format réseau
    // unsigned short htons(unsigned short hostshort)
    // atoi() is a function in the C standard library that converts a string of characters representing an integer value to an actual integer value.
    // Renvoie le port en format réseau en cas de succès et 0 en cas d'échec
    // argv[2] = port du serveur
    server_address.sin_port = htons(atoi(argv[2]));

    // Connecte la socket au serveur
    // connect(int dS, struct sockaddr *aS, socklen_t lgA)
    // Renvoie 0 si la connexion est réussi et -1 si elle échoue
    // dS = descripteur de socket
    // aS = adresse du serveur
    // lgA = taille de la structure aS
    int connection = connect(client_socket, (struct sockaddr*) &server_address, sizeof(server_address));
    if (connection == -1) {
        perror("Erreur lors de la connexion du client au serveur");
        exit(0);
    }

    printf("\033[36m[INFO]\033[0m Dans la liste d'attente...\n");

    //gestion de signaux
    signal(SIGINT, sigint_handler);


    // Cast en long pour éviter un warning
    pthread_t threadReceive;
    pthread_t threadSend;

    pthread_create(&threadReceive, NULL, messageReceive, NULL);
    pthread_create(&threadSend, NULL, messageSend, NULL);

    pthread_join(threadSend, NULL);

    return 0;
}
