#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "config.h"


int sockfd; //serve per setTimeout e altre funzioni dove presente,
//Parametri per il calcolo della formula del timeout adattativo
double estimatedrtt;
double devrtt = 0;
double send_file_timeout = 0.1;
double alfa = 0.125;
double beta = 0.25;

/*
 * Funzione di utility per generare un numero random che servirà per implementare
 * la logica di perdita di pacchetti con probabilità definita dall'utente.
 */

float rnd() {
    return (float) random() / RAND_MAX;
}

/*
 * Funzione per il settaggio del timeout alla socket.
 * La funzione prende in input la quantità di tempo con cui si vuole settare l'attesa della socket.
 */

void setTimeout(double time) {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = time;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

/*
 * Funzione di utility per estrapolare il numero di sequenza da un pacchetto ricevuto
 * affinché si possa implementare l'affidabilità della trasmissione.
 * La funzione prende in input il puntatore al pacchetto ricevuto.
 * La funzione restituisce un intero che rappresenta il numero di sequenza del pacchetto.
 */

int get_packet_index(char *pckt_rcv) {
    char *buf;
    const char s[2] = " ";
    buf = strtok(pckt_rcv, s);
    int i = atoi(buf);
    return i;
}

/*
 * Funzione per il parsing del pacchetto ricevuto per quanto riguarda il payload dello stesso.
 * La funzione prende in input un puntatore al pacchetto ricevuto e l'indice di riferimento del pacchetto .
 * La funzione restituisce la sottostringa del pacchetto contentente il messaggio vero e proprio.
 */

char *parse_pckt_recived(char *pckt_rcv, int index) {
    char c_index[4096];
    sprintf(c_index, "%d", index);
    int st = strlen(c_index) + 1;
    char *start = &pckt_rcv[st];
    char *end = &pckt_rcv[SIZE_MESSAGE_BUFFER];
    char *substr = (char *) calloc(1, end - start + 1);
    memcpy(substr, start, end - start);
    return substr;
}

/*
 * Funzione per il calcolo dei timeout adattativo.
 * La funzione prende in input l'istante iniziale di RTT e l'istante finale di RTT.
 */

void get_adaptative_timeout(double iniziale, double finale) {
    double old_estimated = estimatedrtt;
    double sample_rtt = (finale - iniziale);
    estimatedrtt = ((1 - alfa) * old_estimated) + (alfa * (sample_rtt));
    devrtt = ((1 - beta) * (devrtt)) + (beta * (fabs(((sample_rtt) - estimatedrtt))));
    send_file_timeout = estimatedrtt + (4 * (devrtt));
    printf("VALORI DEL TIMEOUT: %f, devrtt: %f, estimatedrtt: %f, sample_rtt: %f.\n", send_file_timeout, devrtt,
           estimatedrtt, sample_rtt);
}

/*
 * Funzione per il settaggio del timeout alla socket.
 * La funzione prende in input la quantità di tempo con cui vogliamo settare l'attesa della socket
 */

void setAdaptativeTimeout(double time) {
    if (time < MIN_SEND_FILE_TIMEOUT) {
        time = MIN_SEND_FILE_TIMEOUT;
    }
    printf("time: %f\n", time);
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = time * 100000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}
