#include <mictcp.h>
#include <api/mictcp_core.h>
#define TIMEOUT 10
#define MAXPERTES 5


/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */

mic_tcp_sock sock; 
int PA = 0;
int PE = 0;
int perte = 0;  
int nbPertesConsec =0;
float nbPertes = 0.0;
float nbEnvois = 1.0;
float tauxPertes = 0.2;


int mic_tcp_socket(start_mode sm)
{
   int result = -1;
   printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
   result = initialize_components(sm); /* Appel obligatoire */
   set_loss_rate(1);

   sock.fd = 1; 
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
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    return 0;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");  printf(__FUNCTION__); printf("\n");
    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send (int mic_sock, char* mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: "); printf(__FUNCTION__); printf("\n");
    if (sock.fd == mic_sock ) {
        //---------INITIALISATION DU PDU-----------------
        mic_tcp_header header = {0, 0, PE, 0, 0, 0, 0};
        PE = (PE + 1)%2;
        mic_tcp_payload payload = {mesg, mesg_size}; 
        mic_tcp_pdu pdu = {header, payload};
        mic_tcp_sock_addr addr; 

        //------------ENVOI DU PDU------------------------
        IP_send(pdu, addr);
        printf("\nEnvoi message, PE = %d \n", PE); 
        mic_tcp_pdu ack;
        ack.payload.size = 0; 
        printf("avant la boucle\n");
        perte = 0;

        //------------GESTION DES PERTES------------------
        while (IP_recv(&ack, &addr, TIMEOUT) == -1 && ack.header.ack_num != PE)
        {   
            perte = 1; //état de perte
            nbPertesConsec ++; 
            if (nbPertes/nbEnvois >= tauxPertes || nbPertesConsec > MAXPERTES) //Si on a un taux de perte trop important ou bien nous avons plusieurs erreurs consécutives
            {
                printf("Aïe je n'ai pas reçu de ack J'ATTENDS : %d \n", PE);
                IP_send(pdu, addr); //renvoi du PDU
                printf("RENVOI du message, PE = %d \n", PE); 
                nbEnvois = 1.0; 
                nbPertes = 0.0; //On remet à 0 ces variables pour avoir réinitialiser le taux
            }
            else 
            {
                printf("J'ai rien reçu mais je m'en fou, j'ai %d pertes consecutives \n", nbPertesConsec);
                PE = (PE + 1)%2;  //Réincrémentation de PE pour que le récepteur n'ait pas de problème de séquençage 
                nbPertes ++;
                break; 
            }
        }
        printf("TAUX DE PERTES ACTUEL : %f \n", nbPertes/nbEnvois);
        printf("pertes : %f,  envois : %f\n", nbPertes, nbEnvois);
        if (nbPertesConsec > MAXPERTES || perte == 0) { //Si nous n'avons pas de perte on remet à 0 les pertes conséqutives
            nbPertesConsec = 0;
        }
        nbEnvois ++;
        printf("ACK reçu\n\n"); 
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
    int numSeq = pdu.header.seq_num;
    printf("\nJ'ai reçu un message de n° : %d et j'attends : %d \n", numSeq, PA);
    if (numSeq == PA)
    {
        app_buffer_put(pdu.payload);
        PA = (PA + 1)%2;
        printf("J'y mets dans le buffer \n ");
    }
    mic_tcp_header ack_header = {0, 0, 0, numSeq, 0, 1, 0}; 
    ack_header.ack = 1;
    ack_header.ack_num = numSeq;
    mic_tcp_payload ack_payload = {"", 0}; 
    mic_tcp_pdu ack = {ack_header, ack_payload}; 
    printf("J'envoi le ACK n° %d \n\n", numSeq); 

    IP_send(ack, addr);
}
