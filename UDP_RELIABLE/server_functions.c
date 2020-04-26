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
#include "config.h"
#include "common_functions.h"



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
int window_base = 0;                    //Parametro di posizionamento attuale nella spedizione
int s_sockfd;                           //File descriptor della socket usata dai processi figli
int size;                               //Dimensione del file da trasferire
int sockfd;                             //File descriptor di socket
int num_client = 0;                     //Numero di Client connessi
int client_port;                        //Porta che diamo al Client per le successive trasmissioni multiprocesso
int port_number = 0;                    //Variabile di utility per il calcolo delle porte successive da dare al Client
int child_pid_array[MAX_CONNECTION];    //Array dei pid dei processi figli che si assoceranno ai rispettivi Client
ssize_t err;                            //Intero per il controllo della gestione d'errore
double tempo_iniziale;            //Istante iniziale di RTT
double tempo_finale;              //Istante finale di RTT
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
 * Funzione che prima di terminare l'applicativo rilascia le risorse uccidendo tutti i processi figli.
 * La funzione prende in input l'identificativo della shared memory
 */

void error_exit(int shmid) {
    int *p = (int *) shmat(shmid, NULL, SHM_RDONLY);
    if (p == (int *) -1) {
        fprintf(stderr, "Errore nella shmat della child_exit del Server.\n");
        exit(EXIT_FAILURE);
    }
    if (p == NULL) {
        printf("Il Server è stato chiuso con successo.\n");
        exit(EXIT_SUCCESS);
    }
    int n_client = p[0];
    printf("Il Server è stato chiuso con successo.\n");
    for (int i = 1; i < n_client + 1; i++) {
        printf("Sto killando il processo con il pid: %d.\n", p[i]);
        kill(p[i], SIGUSR2);
    }
    //Viene impostato un token per indicare che anche il padre deve fare una exit
    exit(EXIT_SUCCESS);
}

/*
 * Funzione unica per la gestione della logica di errore a cui si aggiunge anche
 * la chiamata della funzione per il rilascio delle connessioni aperte.
 * La funzione prende in input una stringa che verrà mostrata a schermo per descrivere l'errore.
 */

void error(const char *msg) {
    perror(msg);
    error_exit(shmid);
    exit(1);
}


/*
 * Funzione che apre e legge il file dove è presente la lista dei file presenti sul Server
 */

void get_list(int sockfd, struct sockaddr_in servaddr, socklen_t len) {
    int ret;
    //int i;
    //char *string[STRING_LENGHT];
    int fd = open("server_file_list.txt", O_RDONLY|O_CREAT, 0666);
    //FILE *f = fopen(fd, "r");
    if (fd == -1) {
        error("Errore nella open della get_list del Server.");
    }
    size = lseek(fd, 0, SEEK_END);
    /*for (i = 0; i<size; i++)    {

        fscanf(f, "%s", string);
    }*/
    buff_file_list = malloc(size);
    if (buff_file_list == NULL) {
        error("Errore nella malloc del buff_file_list della get_list del Server.");
    }
    bzero(buff_file_list, size);
    lseek(fd, 0, 0);
    ret = read(fd, buff_file_list, size);
    if (ret == -1) {
        error("Errore nella read della get_list del Server.");
    }
    //Si manda il buffer alla socket del Server
    ret = sendto(sockfd, buff_file_list, size, 0, (struct sockaddr *) &servaddr, len);
    if (ret < 0) {
        error("errore nella sendto della get_list del Server");
    }
    free(buff_file_list);
}

/*
 * Funzione per inviare alla socket il buffer dove ho salvato la lunghezza del file di interesse
 */

void send_len_file() {
    //Si manda il buffer alla socket del Client con la lunghezza del file
    if (sendto(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, len) < 0) {
        error("Errore nella sendto della send_len_file del Server.");
    }
}

/*
 * Funzione per inviare alla socket il buffer dove è stato salvato il nome del file di interesse
 */

void send_name() {
    if (sendto(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, len) < 0) {
        error("Errore nella sendto della send_name del Server.");
    }
}

/*
 * Funzione per inviare alla socket un buffer che nello specifico rappresenta PASSWORD3
 * che è una password di utility per il reinvio del nome in caso non fosse disponibile un file cosi nominato.
 */

void send_password(int message) {
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    sprintf(buffer, "%d", message);
    if (sendto(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, len) < 0) {
        error("Errore nella sendto della send_password del Server.");
    }
}

/*
 * Funzione in cui si riceve il messaggio del Client con il nome del file che si è interessati a processare;
 * si apre il file, si ottiene la sua lunghezza e la si salva nel buffer.
 * La funzione restituisce il descrittore al file che stiamo processando.
 */

int get_name_and_size_file() {
    RESUME:
    //Si pulisce il buffer
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //Si riceve il messaggio dal Client con il nome del file da aprire
    if (recvfrom(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, &len) < 0) {
        if (errno == EAGAIN) {
            goto RESUME;
        }
        error("Errore nella recvfrom della get_name_and_size_file del Server.");
    }
    //Viene pulito il buffer dal terminatore di stringa
    int len = strlen(buffer);
    if (buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    //Si apre il file attraverso il nome del file per ottenerne la lunghezza e rimandarla al Client
    int size;
    printf("Il file %s è stato richiesto.\n", buffer);
    //Si controlla se il nome ricevuto corrisponde ad un file presente
    int fd = open(buffer, O_RDONLY, 0666);
    if (fd == -1) {
        printf("Attenzione! Ricevuto un nome sbagliato.\nServer ancora in attesa di un nome di file corretto scelto tra i disponibili.\n");
        send_password(PASSWORD3);
        goto RESUME;
    }
    //In caso il nome abbia riscontro positivo viene mandato un messaggio di ok e si continua l'esecuzione inviando il nome del file accettato
    send_name();
    size = lseek(fd, 0, SEEK_END);
    packet_count = (ceil((size / SIZE_PAYLOAD))) + 1;
    printf("PACKET COUNT :%d\t", packet_count);
    lseek(fd, 0, 0);
    //Viene pulito il buffer e si salva al suo interno la lunghezza del file
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    sprintf(buffer, "%d", size);
    //Si invia il buffer con la lunghezza del file al Client tramite la send_len_file()
    send_len_file();
    return fd;
}

/*
 * Funzione per l'invio dell'ACK.
 * La funzione prende in input il numero di sequenza del pacchetto per cui si vuole notificare un ACK
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
            error("Errore nella sendto della sendACK del Server.");
        }
        return 1;
    } else {
        printf("simulazione pacchetto perso, ack: %d non inviato\n", seq);
        return 0;
    }
}

/*
 * Funzione che controlla che tutti i pacchetti della finestra siano stati inviati correttamente,
 * ovvero che abbiano ricevuto un ACK; in caso contrario ci si preoccupa del rinvio.
 * La funzione prende in input il puntatore alla struttura nella quale è stato spacchettato il file
 * e un offset che è il risultato della divisione con resto tra il numero di pacchetti
 * da inviare e la dimensione della finestra di spedizione.
 */

void check_packet_sended_of_window(struct packet_struct *file_struct, int offset) {
    printf("Sto entrando nella check per l'eventuale ritrasmissione di pacchetti persi.\n");
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
            //Si pulisce il buffer
            bzero(buffer, SIZE_MESSAGE_BUFFER);
            //Si riceve il messaggio dal Client con l'ack del pacchetto ricevuto
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
            //Si setta l'ACK del pacchetto ricevuto correttamente
            SENDEND:
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
 * Funzione che gestisce la ricezione dei pacchetti.
 * Questa funzione prepara il programma a ricevere i pacchetti in modo affidabile,
 * in un ciclo while di dimensione pari al numero di pacchetti totali, apre dei cicli for di dimensione della finestra
 * al termine di ognuno dei quali controlla se si è negli ultimi pacchetti offset.
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
    //DSi deve ricevere fino a che i pacchetti che ricevuti sono meno di quelli totali
    while (count < packet_count || counter < packet_count - 1) {
        printf("sono nel while\n");
        printf("offset : %d\t\t count :%d\t\t packet_count : %d\t\t w_size: %d\n", offset, count, packet_count, w_size);
        printf("counter: %d\n", counter);
        /*
         * Si entra in questo ciclo solo nel caso in cui rimangono al piu offset pacchetti,
         * di conseguenza la dimensione della finestra diventa offset.
         */
        if (packet_count - count <= offset + 1 && offset != 0) {
            printf("sono nel ciclo con offset\n");
            if (WINDOW_SIZE % 2) {
                w_size = offset;
            } else {
                w_size = offset + 1;
            }
        }
        //Si ricevono i pacchetti in cicli della dimensione della finestra di ricezione

        clock_t begin = clock();
        
        for (int i = 0; i < w_size; i++) {
            CICLO:
            printf("sono nel for\n");
            printf("offset : %d\t\t count :%d\t\t packet_count : %d\t\t w_size: %d\n", offset, count, packet_count, w_size);
            printf("counter: %d\n", counter);
            char pckt_rcv[SIZE_MESSAGE_BUFFER];
            char *pckt_rcv_parsed;
            pckt_rcv_parsed = malloc(SIZE_PAYLOAD);
            bzero(pckt_rcv, SIZE_MESSAGE_BUFFER);
            bzero(pckt_rcv_parsed, SIZE_PAYLOAD);
            //Si iniziano a ricevere i pacchetti
            errno = 0;
            int err = recvfrom(sockfd, pckt_rcv, SIZE_MESSAGE_BUFFER, 0, (SA *) &servaddr, &len);
            if (err < 0) {
                if (errno == EAGAIN) {
                    printf("tempo di ricezione scaduto nella recive_UDP_rel_file del Client\n");
                    goto FINE;
                } else {
                    error("Errore nella recvfrom della recive_UDP_rel_file nel Client");
                }
            }
            printf("");
            //temp riceve il numero di sequenza messo nel header del pacchetto
            temp = get_packet_index(pckt_rcv);
            if (temp >= count) {
                count = temp + 1;
            }
            printf("");
            pckt_rcv_parsed = parse_pckt_recived(pckt_rcv, temp);
            printf("");
            if (sendACK(temp)) {
                counter = counter + 1;

                //Copia del contenuto del pacchetto nella struttura ausiliaria
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
        }
        else {
            setTimeout(SEND_FILE_TIMEOUT);
        }
        if (sendto(sockfd, file_struct[seq].buf, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, len) < 0) {
            fprintf(stderr, "Errore nell'invio del pacchetto numero: %d.\n", seq);
            exit(EXIT_FAILURE);
        }
        printf("Sto inviando il pacchetto %d.\n", seq);
        //Si pulisce il buffer
        bzero(buffer, SIZE_MESSAGE_BUFFER);
        //Si riceve il messaggio dal Client con l'ack del pacchetto ricevuto
        errno = 0;
        printf("sto per ricevere\n");
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
        }
        if (lock == 0) {
            printf("Ho ricevuto l'ack del pacchetto %s.\n", buffer);
            /*
             * È una variabile che assume il valore del numero ricevuto con l'ack,
             * che è proprio il numero corrispondente al pacchetto, ora si imposta l'ack a 1
             */
            int check = atoi(buffer);
            if (check >= 0) {
                file_struct[check].ack = 1;
            }
        }
        seq = seq + 1;
    }
    //Alla fine di ogni finestra questa funzione controlla che siano effettivamente arrivati tutti i pacchetti
    check_packet_sended_of_window(file_struct, offset);
    return seq;
}

/*
 * Funzione per l'inizializzazione del processo di invio dei pacchetti.
 * Qui vengono settate le strutture di utility e viene chiamata la funzione per l'invio effettivo dei pacchetti
 * con i parametri giusti in base alla situazione corrente.
 * La funzione prende in input il descrittore al file che si ha intenzione di inviare.
 */

void start_sending_pckt(int fd2) {
    //Inizializzione della struttura
    struct packet_struct file_struct[packet_count];
    //Si puliscono tutti i buffer della struttura
    for (int i = 0; i < packet_count; i++) {
        bzero(file_struct[i].buf, SIZE_MESSAGE_BUFFER);
    }
    int ret;
    char *temp_buf;     //Si alloca un buffer della dimensione del payload
    temp_buf = malloc(SIZE_PAYLOAD);
    if (temp_buf == NULL) {
        error("Errore nella malloc della start_sending_pckt del Server.");
    }
    //Viene letto il file e vengono riempiti i dati della struttura
    for (int i = 0; i < packet_count; i++) {
        bzero(temp_buf, SIZE_PAYLOAD);
        ret = read(fd2, temp_buf, SIZE_PAYLOAD);
        if (ret == -1) {
            error("Errore nella read della start_sending_pckt del Server.");
        }
        //Alla trama del pacchetto viene concatenato il numero di sequenza e uno spazio bianco
        char pkt[SIZE_MESSAGE_BUFFER];

        if (sprintf(pkt, "%d ", i) < 0) {
            error("Errore della sprintf 1 nella start_sending_pckt del Server.");
        }

        if (strcat(pkt, temp_buf) == NULL) {
            error("Errore della strcat nella start_sending_pckt del Server.");
        }
        //Si assegna ad ogni campo della struttura il buffer con il pacchetto corrispondente
        if (sprintf(file_struct[i].buf, "%s", pkt) < 0) {
            error("Errore della sprintf 2 nella start_sending_pckt del Server.");
        }
        file_struct[i].counter = i;
    }

    int seq = 0;
    int offset = packet_count % WINDOW_SIZE;
    /*
     * Si entra in questa casistica se il numero di pacchetti non è multiplo della window_size,
     * dato che si avranno paccheti "in avanzo" alla fine dell'ultimo ciclo
     */
    if (offset > 0) {
        if (packet_count - seq >= offset) {
            printf("packet_count = %d\t\t seq= %d\t\t offset= %d\t\t.\n", packet_count, seq, offset);
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

/*
 * Funzione di utlity per la creazione e il binding delle socket.
 * La funzione prende in input il numero di porta che si vuole associare alla socket che si vuole creare.
 * La funzione restituisce il descrittore alla socket che è stata creata.
 */

int create_socket(int s_port) {
    //Creazione della socket
    int s_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    //Si salva in len la lunghezza della struct della socket
    len = sizeof(servaddr);
    //Controllo d'errore nella creazione della socket
    if (s_sockfd == -1) {
        error("ATTENZIONE! Creazione della socket fallita...");
    } else {
        printf("Socket creata correttamente...\n");
    }
    //Si pulisce la memoria allocata per la struttura della socket
    bzero(&servaddr, sizeof(servaddr));
    //Si settano i parametri della struttura della socket
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(s_port);

    //Binding della socket con controllo d'errore
    if ((bind(s_sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) != 0) {
        error("ATTENZIONE! Binding della socket fallito...");
    } else {
        printf("Socket-Binding eseguito correttamente...\n");
        printf("\nPer spegnere il Server inserire 'close'.\n\n");
    }
    return s_sockfd;
}

/*
 * Funzione per la creazione del file locale dove andare a salvare i pacchetti che vengono ricevuti dal Client nel caso di upload.
 * La funzione prende in input la stringa del nome del file che si ha intenzione di creare.
 * La funzione restituisce il descrittore al file appena aperto.
 */

int create_local_file(const char *file_name) {
    printf("Sto creando il file %s...\n", file_name);
    int fd = open(file_name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        error("Errore nella open in create_local_file del Server.");
    }
    return fd;
}

/*
 * Funzione che permette al termine della ricezione di tutti i pacchetti, avvenuta correttamente, 
 * di scriverli nel file locale che è stato creato con la create_local_file nel caso di upload.
 */

void write_data_packet_on_local_file() {
    for (int i = 0; i < packet_count; i++) {
        int ret = write(fd, buff_file[i], SIZE_PAYLOAD);
        if (ret == -1) {
            error("Errore nella write della write_data_packet_on_local_file del Server.");
        }
    }
}

/*
 * Funzione per la ricezione del buffer in cui si ha la lunghezza del file che il Client vuole inviare nel caso di upload
 */

void receive_len_file() {
    err = recvfrom(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (SA *) &servaddr, &len);
    if (err < 0) {
        error("Errore nella recvfrom della receive_len_file del Server.");
    }
}

/*
 * Funzione che scrive sul file 'file_list.txt'dopo l'upload di un nuovo file sul Server
 */

void write_file_list(){
    FILE *file;
    file = fopen("server_file_list.txt", "a");
    if(file == NULL){
        error("Errore nella fopen della write_file_list del server.");
    }
    else {
        pthread_mutex_lock(&m);
        fprintf(file, "\n%s", pathname);
        pthread_mutex_unlock(&m);
    }
    fclose(file);
}

/*
 * Funzione che processa la ricezione di un file con il settaggio di parametri di utility derivati da scambi
 * di messaggi preliminari al invio dei pacchetti riguardanti il contenuto del file vero e proprio.
 */

void receive_name_and_len_file() {
    //Si pulisce il buffer
    START_RECEIVE_LEN:
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //Si riceve il nome del file
    err = recvfrom(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (SA *) &servaddr, &len);
    if (err < 0) {
        if (errno == EAGAIN) {
            goto START_RECEIVE_LEN;
        }
        error("Errore nella recvfrom della receive_name_and_len_file del Server.");
    }
    //Si salva il nome del file ricevuto in pathname
    printf("Sto ricevendo il file %s...\n", buffer);
    if (strcpy(pathname, buffer) == NULL) {
        error("Errore nella strncpy della receive_name_and_len_file del Server.");
    }
    //Si pulisce il buffer
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //Si riceve la lunghezza del file
    receive_len_file();
    /*
     * Viene allocato uno spazio di memoria delle dimensioni della lunghezza del file
     * in cui verranno messi uno alla volta i pacchetti del file che vengono ricevuti  
     */
    int len_file = atoi(buffer);
    printf("Lunghezza file: %d.\n", len_file);
    buff_file = malloc(len_file);
    packet_count = (ceil((len_file / SIZE_PAYLOAD))) + 1;
    for (int i = 0; i < packet_count; i++) {
        buff_file[i] = mmap(NULL, SIZE_PAYLOAD, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
        if (buff_file[i] == NULL) {
            error("Errore nella mmap del buff_file della receive_name_and_len_file del Server.");
        }
    }
    printf("Numero pacchetti da inviare: %d.\n", packet_count);
    /*
     * Dal momento che si hanno il nome e la lunghezza del file da scaricare si può creare il file locale
     * dove poi verranno inseriti i dati presenti nei pacchetti ricevuti una volta terminata la ricezione
     */
    fd = create_local_file(pathname);
    printf("Ho creato il file.\n");
    printf("Inizio a ricevere i pacchetti in modalità affidabile...\n");
    //Ricezione dei pacchetti in modalità affidabile (fino a che il numero di sequenza è < del numero di pacchetti da ricevere [packet_count])
    recive_UDP_rel_file();
    printf("Ricezione terminata correttamente.\n");
    //Ora che sono stati ricevuti tutti i pacchetti si possono salvare tutti i contenuti dei pacchetti, posti in buff_file, nel file precedentemente creato
    write_data_packet_on_local_file();
    printf("File scritto correttamente.\n");
    //Ora che è stato ricevuto il file si aggiorna il file 'file_list' con il nome del file caricato sul Server
    printf("Aggiorno file_list...\n");
    write_file_list();
    printf("File aggiornato correttamente.\nOperazione di upload completata con successo.\n");
}

/*
 * Funzione per la gestione del download lato Server.
 * La funzione prende in input il descrittore della socket, la struttura della socket e la lunghezza della socket.
 */

void serverDownload() {
    int file_descriptor;
    file_descriptor = get_name_and_size_file();
    start_sending_pckt(file_descriptor);
    printf("File inviato correttamente.\n");
}

/*
 * Funzione per la gestione dell'upload lato Server.
 * La funzione prende in input il descrittore della socket, la struttura della socket e la lunghezza della socket.
 */

void serverUpload(int sockfd, struct sockaddr_in servaddr, socklen_t len) {
    //Si svuota il buffer
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //Si mette la stringa nel buffer
    sprintf(buffer, "%s", "Upload permesso.");
    //Si manda il buffer alla socket del Server
    sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &servaddr, len);
    receive_name_and_len_file();
}

/*
 * Funzione per la gestione della richiesta di lista lato Server.
 * La funzione prende in input il descrittore della socket, la struttura della socket e la lunghezza della socket.
 */

void serverList(int sockfd, struct sockaddr_in servaddr, socklen_t len) {
    //Si mette nel buffer la lista dei file presenti sul Server
    get_list(sockfd, servaddr, len);
}

/*
 * Funzione per la gestione dell'input errato.
 * La funzione prende in input il descrittore della socket, la struttura della socket e la lunghezza della socket.
*/

void func_error(int sockfd, struct sockaddr_in servaddr, socklen_t len) {
    //Si svuota il buffer
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    //Si mette la stringa nel buffer
    sprintf(buffer, "%s", "ATTENZIONE! Comando non riconosciuto.");
    //ManSi manda il buffer alla socket del Server
    sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *) &servaddr, len);
}

/*
 * Funzione che gestisce la richiesta di uscita da un Client che prende come parametri
 * il numero di porta, il file descriptor della socket e il pid del processo padre.
 * Chiude la socket verso il Client e poi manda un segnale al padre che lo gestisce con un handler
 * che decrementa il numero di Client connessi affinchè questo parametro resti sempre aggiornato.
 * La funzione prende in input il numero di porta associato al Client, il descrittore della socket
 * e l'identificativo del processo che gestisce il Client.
 */

void serverExit(int client_port, int socket_fd, pid_t pid) {
    printf("Sto chiudendo la connessione con il Client collegato alla porta: %d.\n", client_port);
    int ret = close(socket_fd);
    if (ret == -1) {
        error("Errore nella chiusura della socket in serverExit del Server.");
    }
    kill(pid, SIGUSR1); //Invia il segnale SIGUSR1 al pid
}

/*
 * Funzione che viene chiamata nell'ambito della gestione dei segnali per decrementare il numero dei Client connessi
 */

void sighandler() {
    num_client = num_client - 1;
}

/*
 * Funzione che gestisce la chiusura del Server, chiude la socket e manda al Client un messaggio
 * che gli consente di capire se il Server non è più in funzione cosi che possa terminare anch'esso la sua esecuzione.
 */

void child_exit_handler() {
    printf("Ho chiuso la socket: %d.\n", sockfd);
    bzero(buffer, SIZE_MESSAGE_BUFFER);
    sprintf(buffer, "%d", PASSWORD);
    //Se ci sono Client connessi gli viene notificata la chiusura del Server
    if (num_client != 0) {
        if (sendto(sockfd, buffer, SIZE_MESSAGE_BUFFER, 0, (struct sockaddr *) &servaddr, len) < 0) {
            error("Errore nella sendto della child_exit_handler del Server.");
        }
    }
    close(sockfd);
    exit(EXIT_SUCCESS);
}

/*
 * Funzione che cattura la stringa 'close' da standard input del Server, legge dalla memoria condivisa
 * i pid di tutti i processi figli associati ai Client e manda il segnale SIGUSR2
 * che è gestito da un handler specifico che provoca la chiusura di questi processi figli.
 * Alla fine della funzione il processo manda un segnale SIGCHLD al processo padre, che ne causa a sua
 * volta la chiusura, prima di chiudere anche se stesso e terminare l'applicativo.
 * La funzione prende in input l'identificativo della shared memory
*/

void child_exit(int shmid) {
    START:
    bzero(exit_buffer, 6);
    fgets(exit_buffer, 6, stdin);
    int *p;
    if (strlen(exit_buffer) == 0) {
        goto START;
    } else {
        if (strncmp("close", exit_buffer, strlen("close")) != 0) {
            printf("Inserire il comando 'close' se si vuole spegnere il Server.\n");
            goto START;
        } else {
            p = (int *) shmat(shmid, NULL, SHM_RDONLY);
            if (p == (int *) -1) {
                error("Errore nella shmat della child_exit del Server.");
            }
            if (p == NULL) {
                printf("Il Server è stato chiuso con successo.\n");
                kill(getppid(), SIGCHLD);
                exit(EXIT_SUCCESS);
            }
            int n_client = p[0];
            printf("Chiudo le connessioni verso i Client.\n");
            for (int i = 1; i < n_client + 1; i++) {
                printf("Sto killando il processo con il pid: %d.\n", p[i]);
                kill(p[i], SIGUSR2);
            }
            printf("Il Server è stato chiuso con successo.\n");
            //Si imposta un token per indicare che anche il padre deve fare una exit
            kill(getppid(), SIGCHLD);
            exit(EXIT_SUCCESS);
        }
    }
}

/*
 * Handler che provoca la chiusura del processo padre alla ricezione del segnale SIGCHLD
 */

void exit_handler() {
    sleep(1);
    exit(EXIT_SUCCESS);
}
