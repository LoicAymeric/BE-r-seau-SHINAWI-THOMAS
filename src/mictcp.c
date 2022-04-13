#include <mictcp.h>
#include <api/mictcp_core.h>
#define TIMEOUT 5
#define ECHECSMAX 3


/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */

mic_tcp_sock sock; 
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//-----------------------------------------------------------
int PA = 0;
int PE = 0;
int perte = 0; 

int maxPertesConsec = 5;
float tauxPertesAdmis = 0.001;
int tauxPertesSimule = 20; //En pourcent
 
int nbPertesConsec = 0;
float nbSkip = 0.0; //nb de pertes admissibles    
float nbMessages = 0.0; //nb d'envois sans les remises 

float nbEnvois = 0.0; //nb total de messages envoyés (en comptant les remises)
float nbPertes = 0.0; //nb totales de pertes

int nbAcks = 0;

int attentesServeur = 0; //% de pertes


//-----------------------------------------------------------

int mic_tcp_socket(start_mode sm)
{
   int result = -1;
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   result = initialize_components(sm); /* Appel obligatoire */
   set_loss_rate(tauxPertesSimule);

   sock.fd = result; 
   sock.state = CLOSED ; 
   if (result != -1) return sock.fd;
   else return -1; 
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   return 0;
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr* addr)
{
    sock.state = IDLE;

    while (sock.state != ESTABLISHED) {pthread_cond_wait(&cond, &mutex);}
    printf("---------CONNEXION ETABLIE---------\n\n");
    return 0;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    int nbEchecs = 0; 

    //---------INITIALISATION DU SYN-----------------
    mic_tcp_header header = {0, 0, 0, 0, 0, 0, 0};
    header.syn = 1; 
    char attClient[3]; 
    sprintf(attClient, "%d", tauxPertesSimule/2); 
    mic_tcp_payload payload = {attClient, 3};
    mic_tcp_pdu syn = {header, payload};
    

    //----------ENVOI SYN------------------
    IP_send(syn, addr); 
    sock.state = SYN_SENT;
    printf("[MIC-TCP] Envoi du PDU SYN \n");
    mic_tcp_pdu synack;
    synack.payload.size = 0; 
    while (IP_recv(&synack, &addr, TIMEOUT) == -1)
    {
        if (nbEchecs >= ECHECSMAX)
        {
            printf("CONNEXION IMPOSSIBLE AU BOUT DE PLUSIEURS ESSAIS\n");
            return -1; 
        }
        nbEchecs++;

        IP_send(syn, addr);
        
    }
    if (synack.header.syn != 1 || synack.header.ack != 1)
    {
        printf("PAS DE SYNACK\n");
        return -1; 
    }
    printf("[MIC-TCP] Reception d'un SYNACK\n");

    //--------INIT TAUX DE PERTES-----------
    int attFin = atoi(synack.payload.data);
    tauxPertesAdmis = (float)attFin / 100.0;

    //---------INITIALISATION DU ACK--------------
   
    header.syn = 0; 
    header.ack = 1; 
    mic_tcp_pdu ack = {header, payload};
    printf("[MIC-TCP] Envoi du PDU ACK \n"); 
    IP_send(ack, addr); 
    printf("---------CONNEXION ETABLIE---------\n\n");
    sock.state = ESTABLISHED; 
    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    if (sock.fd == mic_sock ) {
        //---------INITIALISATION DU PDU-----------------
        mic_tcp_header header = {0, 0, PE, 0, 0, 0, 0};
        PE = (PE + 1)%2;
        mic_tcp_payload payload = {mesg, mesg_size}; 
        mic_tcp_pdu pdu = {header, payload};
        mic_tcp_sock_addr addr; 

        //------------ENVOI DU PDU------------------------
        IP_send(pdu, addr);
        mic_tcp_pdu ack;
        ack.payload.size = 0; 

        perte = 0; //etat de perte
        nbEnvois++;
        nbMessages++;

        //-------------CONSOLE--------------------
        printf("[MICTCP] (Taux de pertes simulé : %d%, taux de pertes admis : %f%)\n", tauxPertesSimule, tauxPertesAdmis*100);
        printf("Message n°%f, PE = %d\n", nbMessages, PE);
        printf("%d pertes consécutives, taux de pertes actuel : %f\n", nbPertesConsec, nbSkip/nbMessages);
        printf("%f messages reçus, %f pertes ignorées\n", nbEnvois-nbPertes, nbSkip);
        printf("%f paquets envoyés, %f pertes recensées, %f paquets renvoyés\n", nbEnvois, nbPertes, nbEnvois-nbMessages);
        printf("###############\n\n\n");

        //------------GESTION DES PERTES------------------
        while (IP_recv(&ack, &addr, TIMEOUT) == -1 && ack.header.ack_num != PE)
        {   
            perte = 1; //état de perte
            nbPertes++;
            nbPertesConsec ++;

            //Si la vidéo comporte trop de pertes
            if (nbSkip/nbMessages >= tauxPertesAdmis || nbPertesConsec > maxPertesConsec)
            {
                IP_send(pdu, addr); //renvoi du PDU
                nbEnvois++;
            }

            //Si on peut se permettre d'ignorer le paquet
            else 
            {
                PE = (PE + 1)%2;  //Réincrémentation de PE pour que le récepteur n'ait pas de problème de séquençage 
                nbSkip ++;
                
                
                break; 
            }
        }

        if (nbPertesConsec > maxPertesConsec || perte == 0) { //Si nous n'avons pas de perte on remet à 0 les pertes conséqutives
            nbPertesConsec = 0;
        }

        return 0;

    }

    else
    return -1;
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv (int socket, char* mesg, int max_mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    if (sock.fd == socket ) {
        mic_tcp_payload p = {mesg, max_mesg_size};
        int rcv_size = app_buffer_get(p);
        return rcv_size;
    }
    else return -1;

}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close (int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  "); printf(__FUNCTION__); printf("\n");
    
    return 0;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    mic_tcp_header header = {0, 0, 0, 0, 0, 0, 0}; 
    mic_tcp_payload payload = {"", 0};
    mic_tcp_pdu ack;
    mic_tcp_pdu synack;

    //DANS L'ETABLISSEMENT DE CONNEXION
    if (sock.state != ESTABLISHED) {
        if (pdu.header.syn == 1) {
            header.syn = 1;
            header.ack = 1;
            synack.header = header;
            int attClient = atoi(pdu.payload.data);
            char attFin[3]; 
            sprintf(attFin, "%d", (attentesServeur+attClient)/2);  
            strcpy(payload.data, attFin); 
            payload.size = 3;
            synack.payload = payload; 
            IP_send(synack, addr);
            sock.state = SYN_RECEIVED;
        }
        else if (pdu.header.ack == 1) {
            sock.state = ESTABLISHED;
            pthread_cond_broadcast(&cond);
        }
    }

    //DANS LE TRANSFERT DE DONNEES
    else {
        int numSeq = pdu.header.seq_num;
        printf("\nJ'ai reçu un message de n° : %d et j'attends : %d \n", numSeq, PA);
        if (numSeq == PA)
        {
            app_buffer_put(pdu.payload);
            PA = (PA + 1)%2;
            printf("J'y mets dans le buffer \n ");
        }
        
        header.ack = 1;
        header.ack_num = numSeq;    
        ack.header = header;
        ack.payload = payload; 
        printf("J'envoi le ACK n° %d \n", numSeq); 
        IP_send(ack, addr);
        nbAcks++;
        printf("J'ai envoyé %d acks\n\n", nbAcks);
    }
}
