//CLIENT

#include<netdb.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<netdb.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<math.h>
#include<sys/mman.h>
#include<sys/time.h>
#include <errno.h>
#include <pthread.h>
#include "../config.h"
#include "../common_functions.h"
#include "../client_functions.h"


struct timeval t;                   //Struttura per calcolare il tempo trascorso
struct sockaddr_in servaddr;        //Struct di supporto della socket
socklen_t len;                      //Lunghezza della struct della socket
char file_name[128];                //Buffer per salvare il nome del file
char buffer[SIZE_MESSAGE_BUFFER];   //Buffer unico per le comunicazioni
char **buff_file;                   //Buffer per il contenuto dei pacchetti
char *buff_file_list;               //Buffer per il contenuto della lista di file
int sockfd;                         //File descriptor della socket
ssize_t err;                        //Variabile per controllo di errore
int packet_count;                   //Numero di pacchetti da inviare
int size;                           //Dimensione del file da trasferire

int main() {
    //Creazione della socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    //Lunghezza della struct della socket
    len = sizeof(servaddr);
    //Controllo d'errore nella creazione della socket
    if (sockfd == -1) {
        error("ATTENZIONE! Creazione della socket fallita...");
    } else {

        printf("Socket creata correttamente...\n");
    }
    //Viene pulita la memoria allocata per la struttura del server
    bzero(&servaddr, sizeof(len));
    //Si settano i parametri della struttura del server
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    /*
     * Viene pulita l'area di memoria destinata al messaggio da scambiare tra client e server
     * Invio di un pacchetto di presentazione del Client al Server
     */
    err = sendto(sockfd, buffer, sizeof(buffer), 0, (SA *) &servaddr, len);
    if (err < 0) {
        error("Errore nella sendto del main del Client.");
    }
    printf("Inserisci un comando tra: \n1) exit\n2) list\n3) download \n4) upload\n");
    //Si pulisce il buffer
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //Si riceve la porta con la quale ci si connetterà al Server
    err = recvfrom(sockfd, buffer, sizeof(buffer), 0, (SA *) &servaddr, &len);
    if (err < 0) {
        error("Errore nella recvfrom del main del Client.");
    }
    close(sockfd);
    int port_number = atoi(buffer);
    //Se si riceve un port number = PASSWORD2 il Server sta comunicando che è sovraccarico
    if (port_number == PASSWORD2) {
        error("ATTENZIONE! Impossibile collegarsi al Server, limite di connessioni superato.");
    }
    sockfd = create_socket(port_number);
    //Corpo del Client
    while (1) {
        //Si pulisce il buffer
        bzero(buffer, SIZE_MESSAGE_BUFFER);
        //Si riempie il buffer con la stringa inserita in stdin
        fgets(buffer, SIZE_MESSAGE_BUFFER, stdin);

        //Si confronta il buffer con exit
        if (strncmp("exit", buffer, strlen("exit")) == 0) {
            printf("Il Client sta chiudendo la connessione...\n");
            clientExit(sockfd);
            return 0;
        }
        //Si confronta il buffer con list
        else if (strncmp("list", buffer, strlen("list")) == 0) {
            clientList(sockfd);
            //Si stampa la risposta del Server
            printf("Lista dei file disponibili nel Server:\n%s\n", buffer);
        }
        //Si confronta il buffer con download
        else if (strncmp("download", buffer, strlen("download")) == 0) {
            clientDownload(sockfd);
        }
        //Si confronta il buffer con upload
        else if (strncmp("upload", buffer, strlen("upload")) == 0) {
            clientUpload(sockfd);
        }
        //L'input inserito non è valido
        else {
            printf("Inserisci un comando valido tra list, upload, download e exit.\n");
        }
    }
}
