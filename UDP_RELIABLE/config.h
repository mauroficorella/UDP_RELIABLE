#define PORT 8090                       //Porta di default per l'inizio delle conversazioni client-server
#define SIZE_PAYLOAD 1024               //Dimensione del payload nel pacchetto UDP affidabile
#define SIZE_MESSAGE_BUFFER 1064        //Diensione totale del messaggio che inviamo nell'applicativo
#define LOSS_PROBABILITY 20             //Probabilità di perdita
#define SEND_FILE_TIMEOUT 100000        //Timeout di invio
#define RECV_FILE_TIMEOUT 300000        //Timeout di ricezione
#define WINDOW_SIZE 85                   //Dimensione della finestra di spedizione
#define SA struct sockaddr              //Struttura della socket
#define ADAPTATIVE_TIMEOUT 1            //Flag per settare ad adattativo il time
#define MAX_CONNECTION 5                //Numero massimo di connessioni accettate dal server
#define PASSWORD 19051994               //Pw di utility per gestire la chiusura del server
#define PASSWORD2 11011995              //Pw di utility per gestire l'impossibilità di aggiungere un client
#define PASSWORD3 15061995              //Pw di utility per il reinvio del nome in caso non fosse disponibile un file cosi nominato
#define MIN_SEND_FILE_TIMEOUT 0.005     //Minimo timeout di ricezione
//#define STRING_LENGHT 4096              //Dimensione standard per la stringa usata in get_list per leggere le stringhe dentro i file_list.txt
