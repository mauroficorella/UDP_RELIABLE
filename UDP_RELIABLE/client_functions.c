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
#include "common_functions.h"
#include "config.h"


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
int window_base = 0;                //Parametro di posizionamento attuale nella spedizione
int size;                           //Dimensione del file da trasferire
double tempo_iniziale;               //Istante iniziale di RTT
double tempo_finale;                 //Istante finale di RTT
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;    //Identificativo del semaforo

/*
 * Struttura di utility per implementare il pacchetto UDP affidabile
 */

struct packet_struct {
    int counter;
    char buf[SIZE_MESSAGE_BUFFER];
    int ack;
};

/*
 * Funzione unica per la gestione della logica di errore.
 * La funzione prende in input una stringa che verrà mostrata a schermo per descrivere l'errore.
 */

void error(const char *msg) {
    perror(msg);
    close(sockfd);
    exit(1);
}

/*
 * Funzione che permette al termine della ricezione di tutti i pacchetti, avvenuta correttamente,
 * di scriverli nel file locale che viene creato con la create_local_file nel caso di download.
 * La funzione prende in input il descrittore al file su cui si ha intenzione di scrivere.
 */

void write_data_packet_on_local_file(int fd) {
    for (int i = 0; i < packet_count; i++) {
        int ret = write(fd, buff_file[i], SIZE_PAYLOAD);
        if (ret == -1) {
            error("Errore nella write della write_data_packet_on_local_file del Client.");
        }
    }
}

/*
 * Funzione per l'invio dell'ACK.
 * La funzione prende in input il numero di sequenza del pacchetto per cui si vuole notificare un ACK.
 */

int sendACK(int seq) {
    int loss_prob;
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    sprintf(buffer, "%d", seq);
    if (seq > packet_count - WINDOW_SIZE - 1) {
        loss_prob = 0;
    } else {
        loss_prob = LOSS_PROBABILITY;
    }
    float ran = rnd();
    ran = ran * 100;
    if (ran < (100 - loss_prob)) {
        err = sendto(sockfd, buffer, 32, 0, (SA *) &servaddr, len);
        printf("Sto inviando l'ACK: %d.\n", seq);
        if (err < 0) {
            error("Errore nella sendto della sendACK del Client.");
        }
        return 1;
    } else {
        printf("simulazione pacchetto perso, ack: %d non inviato\n", seq);
        return 0;
    }
}

/*
 * Funzione che apre e legge il file dove è presente la lista dei file presenti sul Client.
 */

void get_list() {
    int ret;
    int fd = open("client_file_list.txt", O_RDONLY, 0666);
    if (fd == -1) {
        error("Errore nella open della get_list del Server.");
    }
    size = lseek(fd, 0, SEEK_END);
    buff_file_list = malloc(size);
    if (buff_file_list == NULL) {
        error("Errore nella malloc del buff_file_list della get_list del Server.");
    }
    lseek(fd, 0, 0);
    ret = read(fd, buff_file_list, size);
    if (ret == -1) {
        error("Errore nella read della get_list del Server.");
    }
    printf("Lista file disponibili nel Client:\n%s\n", buff_file_list);
}

/*
 * Funzione che gestisce la ricezione dei pacchetti.
 * Questa funzione prepara il programma a ricevere i pacchetti in modo affidabile,
 * in un ciclo while di dimensione pari al numero di pacchetti totali apre dei cicli for
 * di dimensione della finestra al termine di ognuno dei quali controlla se si è negli ultimi pacchetti offset.
 * In ogni passo del ciclo manda l'ack del pacchetto e salva il contenuto nelle strutture ausiliare.
 */

void recive_UDP_rel_file() {
    /*
     * Per avere massima affidabilità sul numero di sequenza del pacchetto che si sta ricevendo
     * vengono usate 3 variabili di conteggio che variano a seconda dello scenario.
     */
    int count = 0;
    int counter = 0;
    int temp = 0;
    int offset = 0;
    int w_size = WINDOW_SIZE;
    offset = packet_count % WINDOW_SIZE;
    struct packet_struct file_struct[packet_count];
    setTimeout(RECV_FILE_TIMEOUT);
    //Si deve ricevere fino a che i pacchetti ricevuti sono meno di quelli totali
    while (count < packet_count || counter < packet_count - 1) {
        printf("sono nel while\n");
        printf("offset : %d\t\t count :%d\t\t packet_count : %d\t\t w_size: %d\n", offset, count, packet_count, w_size);
        printf("counter: %d\n", counter);
        /*
         * Si entra in questo ciclo solo nel caso in cui rimangono al piu offset pacchetti,
         * di conseguenza la dimensione della finestra diventa offset
         */
        if (packet_count - count <= offset + 1 && offset != 0) {
            printf("sono nel ciclo con offset\n");
            if (WINDOW_SIZE % 2) {
                w_size = offset;
            } else {
                w_size = offset + 1;
            }
        }

        clock_t begin = clock();

        //Si ricevono i pacchetti in cicli della dimensione della finestra di ricezione
        for (int i = 0; i < w_size; i++) {
            CICLO:
            printf("sono nel for\n");
            printf("offset : %d\t\t count :%d\t\t packet_count : %d\t\t w_size: %d\n", offset, count, packet_count, w_size);
            printf("counter: %d\n", counter);
            char pckt_rcv[SIZE_MESSAGE_BUFFER];
            char *pckt_rcv_parsed;
            pckt_rcv_parsed = malloc(SIZE_PAYLOAD);
            bzero(pckt_rcv, SIZE_MESSAGE_BUFFER); //la funzione bzero cancella SIZE_MESSAGE_BUFFER bytes dalla memoria partendo dalla locazione puntata da pckt_rcv
            bzero(pckt_rcv_parsed, SIZE_PAYLOAD); //idem come sopra
            //Si iniziano a ricevere i pacchetti
            errno = 0;
            int err = recvfrom(sockfd, pckt_rcv, SIZE_MESSAGE_BUFFER, 0, (SA *) &servaddr, &len); //restituisce il numero di bytes ricevuti in caso di successo altrimenti -1 in caso di errore
            if (err < 0) {
                if (errno == EAGAIN) { //è scaduto il timer di ricezione
                    printf("tempo di ricezione scaduto nella recive_UDP_rel_file del Client\n");
                    goto FINE;
                } else {
                    error("Errore nella recvfrom della recive_UDP_rel_file nel Client");
                }
            }
            printf("");
            //temp riceve il numero di sequenza messo nell'header del pacchetto
            temp = get_packet_index(pckt_rcv);
            if (temp >= count) {
                count = temp + 1;
            }
            printf("");
            pckt_rcv_parsed = parse_pckt_recived(pckt_rcv, temp);
            printf("");
            if (sendACK(temp)) {
                counter = counter + 1;
                //Si copia il contenuto del pacchetto nella struttura ausiliaria
                if (strcpy(file_struct[temp].buf, pckt_rcv_parsed) == NULL) {
                    printf("Errore nella prima strcpy del pacchetto %s nella recive_UDP_rel_file del Client.",
                           pckt_rcv_parsed);
                    exit(EXIT_FAILURE);
                }
                if (strcpy(buff_file[temp], file_struct[temp].buf) == NULL) {
                    printf("Errore nella seconda strcpy del pacchetto %s, %s nella recive_UDP_rel_file del Client.",
                           buff_file[temp], file_struct[temp].buf);
                    exit(EXIT_FAILURE);
                }
                printf("\t\tPacchetto ricevuto numero di seq: %d.\n", temp);
            } else {
                goto CICLO;
            }
            FINE:
            printf("");
        }
        clock_t end = clock();
        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        printf("Il tempo impiegato è: %f\n", time_spent);
    }
    return;
}


/*
 * Funzione per la creazione del file locale dove andare a salvare i pacchetti che si ricevono dal Client nel caso di upload.
 * La funzione restituisce il descrittore al file appena aperto.
 */

int create_local_file() {
    printf("Sto creando il file %s...\n", file_name);
    int fd = open(file_name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        error("Errore nella open in create_local_file del Client.");
    }
    return fd;
}

/*
 * Funzione di utlity per la creazione e il binding delle socket.
 * La funzione prende in input il numero di porta che si vuole associare alla socket si vuole creare.
 * La funzione restituisce il descrittore alla socket che abbiamo creato.
 */

int create_socket(int c_port) {
    //Creazione della socket
    int c_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    //Si salva in len la lunghezza della struct della socket
    len = sizeof(servaddr);
    //Controllo d'errore nella creazione della socket
    if (sockfd == -1) {
        error("ATTENZIONE! Creazione della socket fallita...");
    } else {
        printf("Socket creata correttamente...\n");
    }
    //Viene pulita la memoria allocata per la struttura
    bzero(&servaddr, sizeof(servaddr));
    //Vengono settati i parametri della struttura
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(c_port);
    //Binding della socket con controllo d'errore
    return c_sockfd;
}

/*
 * Funzione per inviare alla socket il buffer dove è stata salvata la lunghezza del file di interesse.
 */

void send_len_file() {
    //Il buffer viene mandato alla socket del Server con la lunghezza del file
    if (sendto(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, len) < 0) {
        error("Errore nella sento della send_len_file del Client.");
    }
}

/*
 * Funzione in cui si riceve il messaggio del Client con il nome del file che si è interessati processare;
 * si apre il file, si ottiene la sua lunghezza e la si salva nel buffer.
 * La funzione restituisce il descrittore al file che si sta processando.
 */

int get_name_and_size_file() {
    int size;
    ST:
    //Viene ripulito il buffer
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //Il buffer viene riempito con la stringa inserita in stdin
    fgets(buffer, SIZE_MESSAGE_BUFFER, stdin);
    int len_name = strlen(buffer);
    bzero(file_name, 128);
    strncpy(file_name, buffer, len_name - 1);
    //Si calcola la lunghezza del file per calcolare anche packet_count
    printf("Sto aprendo il file %s...\n", file_name);
    int fd = open(file_name, O_RDONLY, 0666);
    if (fd == -1) {
        printf("ATTENZIONE! File inesistente.\nInserire un nome tra quelli presenti nella lista dei disponibili.\n");
        goto ST;
    }
    //Viene inviato il messaggio con il nome del file che si vuole caricare nel Server
    err = sendto(sockfd, file_name, sizeof(file_name), 0, (SA *) &servaddr, len);
    if (err < 0) {
        error("Errore nella sendto della get_name_and_size_file del Client.");
    }
    size = lseek(fd, 0, SEEK_END);
    packet_count = (ceil((size / SIZE_PAYLOAD))) + 1;
    printf("Numero di pacchetti da caricare: %d.\n", packet_count);
    lseek(fd, 0, 0);
    //Viene ripulito il buffer
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //La lunghezza del file viene messa nel buffer
    sprintf(buffer, "%d", size);
    //Si invia il buffer con la lunghezza del file al Server tramite la send_len_file()
    send_len_file();
    return fd;
}

/*
 * Funzione che controlla che tutti i pacchetti della finestra siano stati inviati correttamente,
 * ovvero che abbiano ricevuto un ACK; in caso contrario ci si preoccupa del rinvio.
 * La funzione prende in input il puntatore alla struttura nella quale abbiamo è stato spacchettato il file
 * e un offset che è il risultato della divisione con resto tra il numero di pacchetti
 * da inviare e la dimensione della finestra di spedizione.
 */

void check_packet_sended_of_window(struct packet_struct *file_struct, int offset, int seq) {
    printf("Sto entrando nella check per l'eventuale ritrasmissione di pacchetti persi seq = %d.\n", seq);
    //Checking degli ACK della finestra
    for (int i = window_base; i < (window_base + offset); i++) {
        if (file_struct[i].ack != 1) {
            //Invio di nuovo il pacchetto
            RESEND:
            if (sendto(sockfd, file_struct[i].buf, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, len) < 0) {
                fprintf(stderr, "Errore nell'invio del pacchetto numero: %d.\n", i);
                exit(EXIT_FAILURE);
            }
            printf("Sto inviando nuovamente il pacchetto %d.\n", i);
            //Viene pulito il buffer
            bzero(buffer, SIZE_MESSAGE_BUFFER);
            //Viene ricevuto il messaggio dal Client con l'ack del pacchetto ricevuto
            if (recvfrom(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, &len) < 0) {
                if (errno == EAGAIN) {
                    if (i == packet_count - 1) {
                        goto SENDEND;
                    }
                    usleep(50);
                    goto RESEND;
                } else {
                    error("errore nella recv from della check_packet_sended_of_window");
                }
            }
            SENDEND:
            //Viene settato l'ACK del pacchetto ricevuto correttamente
            printf("Ho ricevuto l'ack del pacchetto ritrasmesso %s.\n", buffer);
            int check;
            check = atoi(buffer);
            if (check > 0) {
                file_struct[check].ack = 1;
            }
        }
    }
}

/*
 * Funzione che gestisce l'invio dei pacchetti.
 * La funzione prende in input il puntatore alla struttura nella quale è stato spacchettato il file,
 * l'indice di sequenza e il resto della divisione tra numero di pacchetti da inviare e la dimensione della finestra.
 * La funzione restituisce il numero di sequenza attuale.
 */

int send_packet(struct packet_struct *file_struct, int seq, int offset) {
    int lock = 0;
    window_base = seq;
    printf("Probabilità di perdita: %d percento.\n", LOSS_PROBABILITY);
    for (int i = 0; i < offset; i++) {
        lock = 0;
        //Viene impostato l'ack del pacchetto che si sta inviando come 0, verrà settato a 1 una volta ricevuto l'ack dal Client
        file_struct[seq].ack = 0;
        if (ADAPTATIVE_TIMEOUT) {
            setAdaptativeTimeout(send_file_timeout);
            int ret = gettimeofday(&t, NULL);
            if (ret == -1) {
                error("Errore nella gettimeofday 1 nella send_packet del Server.");
            }
            tempo_iniziale = t.tv_sec + (t.tv_usec / 1000000.0);
        } else {
            setTimeout(SEND_FILE_TIMEOUT);
        }
        if (sendto(sockfd, file_struct[seq].buf, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, len) < 0) {
            fprintf(stderr, "Errore nell'invio del pacchetto numero: %d.\n", seq);
            exit(EXIT_FAILURE);
        }
        printf("Sto inviando il pacchetto %d.\n", seq);

        //Viene pulito il buffer
        bzero(buffer, SIZE_MESSAGE_BUFFER);
        //Si riceve il messaggio dal Client con l'ack del pacchetto ricevuto
        errno = 0;
        int err = recvfrom(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (SA *) &servaddr, &len);
        if (err < 0) {
            if (errno == EAGAIN) {
                lock = 1;
                printf("il pacchetto è andato perso, ack: %d non ricevuto\n", seq);
            } else {
                error("Errore nella recvfrom della send_packet del Server.");
            }
        }
        //Questa porzione di codice è dedicata alla logica che fa variare il timeout a seconda del tempo degli invii precedenti
        if (ADAPTATIVE_TIMEOUT) {
            int ret = gettimeofday(&t, NULL);
            if (ret == -1) {
                error("Errore nella gettimeofday 2 nella send_packet del Server.");
            }
            tempo_finale = t.tv_sec + (t.tv_usec / 1000000.0);
            get_adaptative_timeout(tempo_iniziale, tempo_finale);
            //printf("Tempo iniziale:%f\t\t tempo finale : %f\t\t\n", tempo_iniziale, tempo_finale);
        }
        if (lock == 0) {
            printf("Ho ricevuto l'ack del pacchetto %s.\n", buffer);
            /*
             * È una variabile che assume il valore del numero ricevuto con l'ack,
             * che è proprio il numero corrispondente al pacchetto, ora viene impostato l'ack a 1
             */
            int check = atoi(buffer);
            if (check >= 0) {
                file_struct[check].ack = 1;
            }
        }
        seq = seq + 1;
    }
    //Alla fine di ogni finestra questa funzione controlla che siano effettivamente arrivati tutti i pacchetti
    check_packet_sended_of_window(file_struct, offset, seq);
    return seq;
}


/*
 * Funzione per l'inizializzazione del processo di invio dei pacchetti.
 * Qui vengono settate le strutture di utility e viene chiamata la funzione per l'invio effettivo dei pacchetti
 * con i parametri giusti in base alla situazione corrente.
 * La funzione prende in input il descrittore al file che abbiamo intenzione di inviare.
 */

void start_sending_pckt(int fd) {
    //Si inizializza la struttura
    struct packet_struct file_struct[packet_count];
    for (int i = 0; i < packet_count; i++) {
        //Vengono puliti tutti i buffer della struttura
        bzero(file_struct[i].buf, SIZE_MESSAGE_BUFFER);
    }
    int ret;
    //Viene allocato un buffer della dimensione del payload
    char *temp_buf;
    temp_buf = malloc(SIZE_PAYLOAD);
    if (temp_buf == NULL) {
        error("Errore nella malloc della start_sending_pckt del Client.");
    }
    //Il file viene letto e vengono riempiti i dati della struttura
    for (int i = 0; i < packet_count; i++) {
        bzero(temp_buf, SIZE_PAYLOAD);
        ret = read(fd, temp_buf, SIZE_PAYLOAD);
        if (ret == -1) {
            error("Errore nella read della start_sending_pckt del Client.");
        }
        
        char pkt[SIZE_MESSAGE_BUFFER];
        
        //Viene concatenato il numero di sequenza e uno spazio bianco alla trama del pacchetto
        if (sprintf(pkt, "%d ", i) < 0) {
            error("Errore della sprintf 1 nella start_sending_pckt del Client.");
        }

        if (strcat(pkt, temp_buf) == NULL) {
            error("Errore della strcat nella start_sending_pckt del Client.");
        }

        //Si assegna ad ogni campo della struttura il buffer con il pacchetto corrispondente
        if (sprintf(file_struct[i].buf, "%s", pkt) < 0) {
            error("Errore della sprintf 2 nella start_sending_pckt del Client.");
        }

        file_struct[i].counter = i;
    }

    int seq = 0;
    setTimeout(SEND_FILE_TIMEOUT);
    int offset = packet_count % WINDOW_SIZE;
    /*
     * Si entra in questa casistica se il numero di pacchetti non è multiplo della window_size,
     * dato che si avranno paccheti "in avanzo" alla fine dell'ultimo ciclo.
     */
    if (offset > 0) {
        if (packet_count - seq >= offset) {
            printf("packet_count= %d\t\t seq= %d\t\t offset= %d\t\t.\n", packet_count, seq, offset);
            while (seq < packet_count - offset) {
                printf("packet_count= %d\t\t seq= %d\t\t offset= %d\t\t.\n", packet_count, seq, offset);
                seq = send_packet(file_struct, seq, WINDOW_SIZE);
            }
        }
        //Vengono inviati gli n offset pacchetti di resto
        printf("packet_count= %d\t\t seq= %d\t\t offset= %d\t\t.\n", packet_count, seq, offset);
        seq = send_packet(file_struct, seq, offset);

    }
    //Si entra in questa casistica se il numero di pacchetti è multiplo di window size
    else {
        while (seq < packet_count) {
            seq = send_packet(file_struct, seq, WINDOW_SIZE);
        }
    }
}


void write_file_list(){
    FILE *file;
    file = fopen("client_file_list.txt", "a");
    if(file == NULL){
        error("Errore nella fopen della write_file_list del client.");
    }
    else {
        pthread_mutex_lock(&m);
        fprintf(file, "\n%s", file_name);
        pthread_mutex_unlock(&m);
    }
    fclose(file);
}

void clientExit(int sockfd) {
    //Viene pulito il buffer
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //Si copia la stringa di uscita nel buffer
    strcpy(buffer, "exit");
    //Viene inviato il messaggio al Server per notificargli la chiusura del Client
    err = sendto(sockfd, buffer, sizeof(buffer), 0, (SA *) &servaddr, len);
    if (err < 0) {
        error("Errore nella sendto nella sezione del servizio di exit del Client.");
    }
    //Chiusura della socket
    sleep(1);
    close(sockfd);
    printf("Client disconnesso.\n");
}

void clientList(int sockfd) {
    //Invio del messaggio al Server che gli notifica il servizio di 'list' richiesto
    err = sendto(sockfd, buffer, sizeof(buffer), 0, (SA *) &servaddr, len);
    if (err < 0) {
        error("Errore nella sendto nella sezione del servizio di list del Client.");
    }
    LIST:
    //Viene pulito il buffer
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //Ricezione del messaggio da parte del Server in cui è presente la risposta al servizio richiesto
    clock_t begin = clock();
    err = recvfrom(sockfd, buffer, sizeof(buffer), 0, (SA *) &servaddr, &len);
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Il tempo impiegato è: %f\n", time_spent);
    if (err < 0) {
        if (errno == EAGAIN) {
            goto LIST;
        }
        error("Errore nella recvfrom nella sezione del servizio di list del Client.");
    }
    //Al ricevimento di un determinato messaggio uguale a PASSWORD il Server comunica che si è disconnesso
    if (atoi(buffer) == PASSWORD) {
        error("ATTENZIONE! Il Server non è più in funzione.");
    }
}

void clientDownload(int sockfd) {
    int fd;
    //Invio del messaggio al Server che gli notifica il servizio di 'download' richiesto
    err = sendto(sockfd, buffer, sizeof(buffer), 0, (SA *) &servaddr, len);
    if (err < 0) {
        error("Errore nella sendto nella sezione del servizio di download del Client.");
    }
    
    DOWNLOAD:
    //Si pulisce il buffer
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //Ricezione della lista di file che si possono scaricare
    err = recvfrom(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (SA *) &servaddr, &len);
    if (err < 0) {
        if (errno == EAGAIN) {
            goto DOWNLOAD;
        }
        error("Errore nella recvfrom nella sezione del servizio di download del Client.");
    }
    //Al ricevimento di un determinato messaggio uguale a PASSWORD il Server comunica che si è disconnesso
    if (atoi(buffer) == PASSWORD) {
        error("ATTENZIONE! Il Server non è più in funzione.");
    }
    printf("Download permesso.\n");
    //Si stampa la lista ricevuta dal Server
    printf("Lista dei file disponibili nel Server:\n%s\n", buffer);
    //Si pulisce il buffer
    LABEL:
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    printf("Inserisci il nome del file da scaricare...\n");
    //Il buffer viene riempito con la stringa inserita in stdin
    fgets(buffer, SIZE_MESSAGE_BUFFER, stdin);
    //Si salva in filename il nome del file che si ha intenzione di scaricare
    int leng = strlen(buffer);
    if (buffer[leng - 1] == '\n') {
        buffer[leng - 1] = '\0';
    }
    strcpy(file_name, buffer);

    //Invio della richiesta con il nome del file da scaricare preso dalla lista ricevuta dal Server
    err = sendto(sockfd, buffer, sizeof(buffer), 0, (SA *) &servaddr, len);
    if (err < 0) {
        error("Errore nella sendto 2 nella sezione del servizio di download del Client.");
    }
    //Si riceve un messaggio dal Server che ci permette di capire se il nome inviato è corretto
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    err = recvfrom(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (SA *) &servaddr, &len);
    if (err < 0) {
        error("Errore nella recvfrom nella sezione del servizio di download del Client.");
    }
    if (atoi(buffer) == PASSWORD3) {
        goto LABEL;
    }
    //Si ripulisce il buffer
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //Si riceve la lunghezza del file
    err = recvfrom(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (SA *) &servaddr, &len);
    if (err < 0) {
        error("Errore nella recvfrom 2 nella sezione del servizio di download del Client.");
    }
    /*
     * Viene allocato uno spazio di memoria delle dimensioni della lunghezza del file
     * in cui verranno messi uno alla volta i pacchetti del file che vengono ricevuti  
     */
    int len_file = atoi(buffer);
    buff_file = malloc(len_file);
    //Con la lunghezza del file si calcola packet_count
    packet_count = (ceil((len_file / SIZE_PAYLOAD))) + 1;
    for (int i = 0; i < packet_count; i++) {
        buff_file[i] = mmap(NULL, SIZE_PAYLOAD, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
        if (buff_file[i] == NULL) {
            error("Errore nella mmap di buff_file nella sezione del servizio di download del Client.");
        }
    }
    printf("Numero pacchetti da ricevere: %d.\n", packet_count);
    /*
     * Dal momento che si hanno il nome e la lunghezza del file da scaricare si può creare il file locale
     * dove poi verranno inseriti i dati presenti nei pacchetti ricevuti una volta terminata la ricezione
     */
    fd = create_local_file();
    printf("Ho creato il file.\n");
    printf("Inzio a ricevere i pacchetti in modalità affidabile...\n");
    //Ricezione dei pacchetti in modalità affidabile (fino a che il numero di sequenza è < del numero di pacchetti da ricevere [packet_count])
    clock_t begin = clock();
    recive_UDP_rel_file();
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Il tempo impiegato è: %f\n", time_spent);
    printf("Ricezione terminata correttamente.\n");
    //Ora che sono stati ricevuti tutti i pacchetti si possono salvare tutti i contenuti dei pacchetti, posti in buff_file, nel file precedentemente creato 
    printf("Scrivo il file...\n");
    write_data_packet_on_local_file(fd);
    printf("File scritto correttamente.\n");
    printf("Aggiorno il file list...\n");
    write_file_list();
    printf("File aggiornato correttamente\nOperazione di download completata con successo.\n");
}

void clientUpload(int sockfd) {
    int fd;
    //Si invia il messaggio al Server per notificargli il servizio di 'upload' richiesto
    err = sendto(sockfd, buffer, sizeof(buffer), 0, (SA *) &servaddr, len);
    if (err < 0) {
        error("Errore nella sendto nella sezione del servizio di upload del Client.");
    }
    UPLOAD:
    //Si riceve il messaggio del Client che notifica che l'upload è permesso
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    err = recvfrom(sockfd, buffer, sizeof(buffer), 0, (SA *) &servaddr, &len);
    if (err < 0) {
        if (errno == EAGAIN) {
            goto UPLOAD;
        }
        error("Errore nella recvfrom nella sezione del servizio di upload del Client.");
    }
    //Al ricevimento di un determinato messaggio uguale a PASSWORD il Server comunica che si è disconnesso
    if (atoi(buffer) == PASSWORD) {
        error("ATTENZIONE! Il Server non è più in funzione.");
    }
    //Si stampa la risposta del Server che ci ha dato l'ok per l'upload
    printf("%s\n", buffer);
    //Si stampa a schermo la lista dei file disponibili
    get_list();
    printf("Inserisci il nome del file da caricare...\n");
    //Si ottengono il nome e le informazioni del file che abbiamo intenzione di caricare nel Server
    fd = get_name_and_size_file();
    //Inizia la trasmissione
    printf("Inizio il caricamento del file...\n");
    clock_t begin = clock();
    start_sending_pckt(fd);
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Il tempo impiegato è: %f\n", time_spent);
    printf("File caricato correttamente, operazione di caricamento completata con successo.\n");
}
