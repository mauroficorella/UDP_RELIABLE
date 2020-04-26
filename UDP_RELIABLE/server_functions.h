//dichiarazioni delle funzioni usate nel server


extern char buffer[SIZE_MESSAGE_BUFFER];       //Buffer unico per le comunicazioni
extern char exit_buffer[6];                    //Buffer di utility per la chiusura del Server
extern char pathname[128];                     //Buffer per il nome del file
extern char **buff_file;                       //Buffer per il contenuto dei pacchetti
extern char *buff_file_list;                   //Buffer per il contenuto della lista di file
extern struct sockaddr_in servaddr;            //Struct di supporto della socket
extern struct timeval t;                       //Struttura per calcolare il tempo trascorso
extern socklen_t len;                          //Lunghezza della struct della socket
extern pid_t parent_pid;                       //Pid del primo processo padre nel main
extern int fd;                                 //File descriptor generico di utility
extern int shmid;                              //Identificativo della memoria condivisa
extern int packet_count;                       //Numero di pacchetti da inviare
extern int window_base;                    //Parametro di posizionamento attuale nella spedizione
extern int s_sockfd;                           //File descriptor della socket usata dai processi figli
extern int size;                               //Dimensione del file da trasferire
extern int sockfd;                             //File descriptor di socket
extern int num_client;                     //Numero di Client connessi
extern int client_port;                        //Porta che viene associata al Client per le successive trasmissioni multiprocesso
extern int port_number;                    //Variabile di utility per il calcolo delle porte successive da dare al Client
extern int child_pid_array[MAX_CONNECTION];    //Array dei pid dei processi figli che si assoceranno ai rispettivi Client
extern ssize_t err;                            //Intero per il controllo della gestione d'errore
extern double tempo_iniziale;            //Istante iniziale di RTT
extern double tempo_finale;              //Istante finale di RTT

extern pthread_mutex_t m;    //Identificativo del semaforo

/*
 * Struttura di utility per implementare il pacchetto UDP affidabile
 */

struct packet_struct {
    int counter;
    char buf[SIZE_MESSAGE_BUFFER];
    int ack;
};

void error_exit(int shmid);
void error(const char *msg);
void get_list(int sockfd, struct sockaddr_in servaddr, socklen_t len);
void send_len_file();
void send_name();
void send_password(int message);
int get_name_and_size_file();
int sendACK(int seq);
void check_packet_sended_of_window(struct packet_struct *file_struct, int offset);
void recive_UDP_rel_file();
int send_packet(struct packet_struct *file_struct, int seq, int offset);
void start_sending_pckt(int fd2);
int create_socket(int s_port);
int create_local_file(const char *file_name);
void write_data_packet_on_local_file();
void receive_len_file();
void write_file_list();
void receive_name_and_len_file();
void serverDownload();
void serverUpload(int sockfd, struct sockaddr_in servaddr, socklen_t len);
void serverList(int sockfd, struct sockaddr_in servaddr, socklen_t len);
void func_error(int sockfd, struct sockaddr_in servaddr, socklen_t len);
void serverExit(int client_port, int socket_fd, pid_t pid);
void sighandler();
void child_exit_handler();
void child_exit(int shmid);
void exit_handler();
