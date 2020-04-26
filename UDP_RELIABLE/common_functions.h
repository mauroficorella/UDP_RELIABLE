//dichiarazioni delle funzioni comuni usate in client e server

extern int sockfd; //serve per setTimeout e altre funzioni dove presente
//Parametri per il calcolo della formula del timeout adattativo
extern double estimatedrtt;
extern double devrtt;
extern double send_file_timeout;
extern double alfa;
extern double beta;

float rnd();
float setTimeout(double time);
int get_packet_index(char *pckt_rcv);
char *parse_pckt_recived(char *pckt_rcv, int index);
void get_adaptative_timeout(double iniziale, double finale);
void setAdaptativeTimeout(double time);