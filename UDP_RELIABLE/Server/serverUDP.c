//SERVER

#include<stdio.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<netdb.h>
#include<string.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/time.h>
#include<signal.h>
#include<math.h>
#include<sys/mman.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<pthread.h>
#include <errno.h>
#include "../config.h"
#include "../common_functions.h"
#include "../server_functions.h"


char buffer[SIZE_MESSAGE_BUFFER];       //Buffer unico per le comunicazioni
char exit_buffer[6];                    //Buffer di utility per la chiusura del Server
char pathname[128];                     //Buffer per il nome del file
char **buff_file;                       //Buffer per il contenuto dei pacchetti
char *buff_file_list;                   //Buffer per il contenuto della lista di file
struct sockaddr_in servaddr;            //Struct di supporto della socket
struct timeval t;                       //Struttura per calcolare il tempo trascorso
socklen_t len;                          //Lunghezza della struct della socket
pid_t parent_pid;                       //Pid del primo processo padre nel main
int fd;                                 //File descriptor generico di utility
int shmid;                              //Identificativo della memoria condivisa
int packet_count;                       //Numero di pacchetti da inviare
int s_sockfd;                           //File descriptor della socket usata dai processi figli
int size;                               //Dimensione del file da trasferire
int sockfd;                             //File descriptor di socket
int client_port;                        //Porta che diamo al Client per le successive trasmissioni multiprocesso
int child_pid_array[MAX_CONNECTION];    //Array dei pid dei processi figli che si assoceranno ai rispettivi Client
ssize_t err;                            //Intero per il controllo della gestione d'errore





int main() {
    //Si inizializza la memoria condivisa che servirà a salvare i pid dei figli
    shmid = shmget(IPC_PRIVATE, sizeof(int) * MAX_CONNECTION, IPC_CREAT | 0666);
    if (shmid == -1) {
        error("Errore nella shmget nel main del Server.");
    }
    //Viene salvato il pid del processo padre in una variabile globale
    parent_pid = getpid();
    //Gestione dei segnali
    signal(SIGUSR1, sighandler);
    signal(SIGCHLD, exit_handler);
    s_sockfd = create_socket(PORT);
    //Prima fork() eseguita qui: viene creato un processo che resta in ascolto per un'eventuale richiesta di chiusura del Server
    pid_t pid = fork();
    if (pid == -1) {
        error("Errore nella prima fork del main del Server.");
    }
    if (pid == 0) {
        signal(SIGUSR1, SIG_IGN);
        child_exit(shmid);
    }
    //CORPO DEL SERVER
    while (1) {
        //Ricezione di un pacchetto di "benvenuto" inviato dal Client che notifica la sua aggiunta alla rete
        bzero(buffer, SIZE_MESSAGE_BUFFER);
        if (recvfrom(s_sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, &len) < 0) {
            error("Errore nella recvfrom nel primo while del Server.");
        }
        //Pulizia del buffer
        bzero(buffer, SIZE_MESSAGE_BUFFER);
        //Aggiornamento del contatore del numero dei Client connessi
        num_client = num_client + 1;
        /*
         * Se il numero di connessioni supera il limite preimpostato viene mandato un messaggio che permette al Client di capire
         * che non gli sarà consentita la connessione
         */
        if (num_client >= MAX_CONNECTION) {
            num_client = num_client - 1;
            printf("ATTENZIONE! Il numero di connessioni supera il limite scelto.\n");
            bzero(buffer, SIZE_MESSAGE_BUFFER);
            sprintf(buffer, "%d", PASSWORD2);
            if (sendto(s_sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, len) < 0) {
                error("Errore nella sendto del primo while del main del Server.");
                while (1) {
                    sleep(1000);
                }
            }
        }
        //Si può concedere la connessione al nuovo Client
        else {
            //Viene creato il nuovo numero di porta
            port_number = port_number + 1;
            client_port = PORT + port_number;
            //Si invia al Client il nuovo numero di porta al quale dovrà connettersi per le successive connessioni
            bzero(buffer, SIZE_MESSAGE_BUFFER);
            sprintf(buffer, "%d", client_port);
            printf("Ci sono %d Client connessi.\n", num_client);
            if (sendto(s_sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, len) < 0) {
                error("Errore nella sendto 2 del primo while del main del Server.");
            }
            /*
             * Viene creato un processo figlio che gestirà la comunicazione con quello specifico Client
             * mentre il padre rimarrà in ascolto di altre eventuali richieste di connessione
             */
            pid_t pid = fork();
            if (pid == -1) {
                error("Errore nella fork del primo while del main del Server.");
            }

            if (pid == 0) {
                //Gestione dei segnali dei processi figli che devono avere una gestione segnali diversa da quella del padre per evitare conflitti
                signal(SIGUSR2, child_exit_handler);
                signal(SIGCHLD, SIG_IGN);
                signal(SIGUSR1, SIG_IGN);
                //Si crea la nuova socket con la quale comunicheranno solo il processo figlio del Server e il nuovo Client e si chiude la precedente
                sockfd = create_socket(client_port);
                close(s_sockfd);
                while (1) {
                    RESTART_SOCKET:
                    //Si pulisce il buffer
                    bzero(buffer, SIZE_MESSAGE_BUFFER);
                    //Si riceve il messaggio del servizio che il Client richiede
                    if (recvfrom(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, &len) < 0) {
                        if (errno == EAGAIN) {
                            goto RESTART_SOCKET;
                        } else {
                            error("Errore nella recvfrom del secondo while del main del Server.");
                        }
                    }
                    /*
                     * Si confronta il messaggio ricevuto e si apre la rispettiva funzione per la exit, la list, la download,
                     * l'upload o per la gestione dell'errore per la ricezione di un comando inserito non valido
                     */
                    if (strncmp("exit", buffer, strlen("exit")) == 0) {
                        printf("Sto processando la 'exit' del Client collegato alla porta: %d.\n", client_port);
                        serverExit(client_port, sockfd, parent_pid);
                        while (1) {
                            sleep(1000);
                        }
                    } else if (strncmp("list", buffer, strlen("list")) == 0) {
                        printf("Sto processando la 'list' del Client collegato alla porta: %d.\n", client_port);
                        serverList(sockfd, servaddr, len);
                    } else if (strncmp("download", buffer, strlen("download")) == 0) {
                        printf("Sto processando la 'download' del Client collegato alla porta: %d.\n", client_port);
                        serverList(sockfd, servaddr, len);
                        //serverDownload(sockfd, servaddr, len);
                        serverDownload();
                    } else if (strncmp("upload", buffer, strlen("upload")) == 0) {
                        printf("Sto processando l' 'upload' del Client collegato alla porta: %d.\n", client_port);
                        serverUpload(sockfd, servaddr, len);
                    } else {
                        func_error(sockfd, servaddr, len);
                    }
                }
            }
            //Si inserisce il pid del processo legato al nuovo Client nella memoria condivisa (nel primo slot)
            child_pid_array[num_client - 1] = pid;
            int *p = (int *) shmat(shmid, NULL, 0);
            if (p == (int *) -1) {
                error("Errore nella shmat 2 nel main del Server.");
            }
            //Nel primo slot della memoria condivisa viene salvato il numero di Client connessi
            p[0] = num_client;
            p[num_client] = pid;
        }
    }
}
