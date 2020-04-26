//dichiarazioni delle funzioni usate nel client

extern struct timeval t;                   //Struttura per calcolare il tempo trascorso
extern struct sockaddr_in servaddr;        //Struct di supporto della socket
extern socklen_t len;                      //Lunghezza della struct della socket
extern char file_name[128];                //Buffer per salvare il nome del file
extern char buffer[SIZE_MESSAGE_BUFFER];   //Buffer unico per le comunicazioni
extern char **buff_file;                   //Buffer per il contenuto dei pacchetti
extern char *buff_file_list;               //Buffer per il contenuto della lista di file
extern int sockfd;                         //File descriptor della socket
extern ssize_t err;                        //Variabile per controllo di errore
extern int packet_count;                   //Numero di pacchetti da inviare
extern int window_base;                //Parametro di posizionamento attuale nella spedizione
extern int size;                           //Dimensione del file da trasferire
extern double tempo_iniziale;        //Istante iniziale di RTT
extern double tempo_finale;          //Istante finale di RTT
extern pthread_mutex_t m;    //Identificativo del semaforo

/*
 * Struttura di utility per implementare il pacchetto UDP affidabile
 */
struct packet_struct {
    int counter;
    char buf[SIZE_MESSAGE_BUFFER];
    int ack;
};

void error(const char *msg);
void write_data_packet_on_local_file(int fd);
int sendACK(int seq);
void get_list();
void recive_UDP_rel_file();
int create_local_file();
int create_socket(int c_port);
void send_len_file();
int get_name_and_size_file();
void check_packet_sended_of_window(struct packet_struct *file_struct, int offset, int seq);
int send_packet(struct packet_struct *file_struct, int seq, int offset);
void start_sending_pckt(int fd);
void write_file_list();
void clientExit(int sockfd);
void clientList(int sockfd);
void clientDownload(int sockfd);
void clientUpload(int sockfd);
