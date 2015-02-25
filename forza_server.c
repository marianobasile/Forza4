#include <stdlib.h>
#include <stdio.h>																							
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h> 

#define DIM_TCP_MSG 1024
#define IP_SIZE 16
#define CLIENT_NAME_LENGTH 20
#define UDP_PORT_LENGTH 7

#define LOGIN 20
#define WHO 21
#define CONNECT 22
#define DISCONNECT 23
#define QUIT 24
#define NO 25
#define YES 26

char * connectedClient[100];

struct Client {											                // STRUTTURA DATI PER MEMORIZZARE INFO RELATIVE AI CLIENT CONNESSI
    char username[CLIENT_NAME_LENGTH];
    char udpPort[UDP_PORT_LENGTH];
    char ipAddress[IP_SIZE];
    int  state;  										                // 1 LIBERO , 0 OCCUPATO 
    char opponent[CLIENT_NAME_LENGTH]; 					    //se state = 0 conterra' il nome dell'avversario						
    int  sock;											                //Socket a cui è connesso
    struct Client* pun;
  };

int conta = 0;

int sendTcpMessage(int sk, char* outboxMsg) {

  int outboxMsgLength;
  int ret;

  outboxMsgLength = htonl(strlen(outboxMsg));

  ret = send(sk, &outboxMsgLength, sizeof(outboxMsgLength), 0);				//Invio della dimensione del messaggio.

  if(ret < 0)
    	return -1;

  if(ret == 0)
    	return 0;

  ret = send(sk, outboxMsg, strlen(outboxMsg), 0);

  if(ret < 0)
    	return -1;

  if(ret == 0)
    	return 0;
  
  return ret;
}

int recvTcpMessage(int sk, char *inboxMsg) {

  int inboxMsgLength;
  int ret;

  memset(inboxMsg,0,sizeof(DIM_TCP_MSG));

  ret = recv(sk, &inboxMsgLength, sizeof(inboxMsgLength), MSG_WAITALL);

  if(ret < 0)
    	return -1;

  if(ret == 0)
    	return 0;

  inboxMsgLength = ntohl(inboxMsgLength);

  ret = recv(sk, inboxMsg, inboxMsgLength, MSG_WAITALL);

  if(ret < 0)
    	return -1;

  if(ret == 0)
    	return 0;

  inboxMsg[inboxMsgLength]='\0';

  return ret;
}

int validateAddress(char *address, int length) {

	int count = 0;
	char * addressPart = strtok(address,".");

	if (addressPart == NULL)
		return -1;
	
	while(addressPart != NULL) {
		count++;
    	if( (atoi(addressPart) < 0) || (atoi(addressPart) > 255) ) 
    			return -1;
    	addressPart = strtok(NULL,".");
	}
		if(count != 4)
				return -1;
	return 0;
}

int getClientAgainFree(struct Client* testa, int sk,char* buff) {

  struct Client *elem;
  char opp[CLIENT_NAME_LENGTH];
  
  elem = testa;
  
  strcpy(opp,buff+3);
  
  while((elem != NULL) && (elem->sock != sk))
    elem = elem->pun;

  memset(buff, 0, DIM_TCP_MSG);
  strcpy(buff,"NO ");
  strcat(buff,"\t\t");
  strcat(buff,elem->username);
  strcat(buff," HA RIFIUTATO L'INVITO AD AVVIARE UNA NUOVA PARTITA\n");
  elem->state = 1;

  elem = testa;
  while((elem != NULL) && (strcmp(elem->username,opp) != 0))
    elem = elem->pun;

  elem->state = 1;
  
  
  return elem->sock;
}

int isLogged(struct Client* lista, int sk) {							//Verifico se Client è loggato sul server //1(si) 0(no)

  struct Client* testa = lista;

  while( (testa != NULL) && (testa->sock != sk) )
    testa = testa->pun;

  if(testa == NULL)
    return 0;
  else
    return 1;
}

int clientState(struct Client* testa, int sk) {

  struct Client *elem;
  elem = testa;

  while( (elem != NULL) && (elem->sock != sk) )
    elem = elem->pun;

  return elem->state;
}

struct Client* removeClient(struct Client* testa, int fd) {
  
  struct Client *elem;
  struct Client *prec;

  prec = NULL;
  elem = testa;

  while( (elem != NULL) && (elem->sock != fd)  )
  { 
    prec = elem;  
    elem = elem->pun; 
  }

  if(elem == NULL)
    return testa;

  printf("%s si e' disconesso\n",elem->username);

  if(elem == testa) 
    testa = testa->pun;
  else
    prec->pun = elem->pun;
   
  free(connectedClient[elem->sock]); 	
  free(elem);
  return testa;
}

void completeLogClient(struct Client *testa, int fd, char *buff) {

  //cerco la struttura realtiva al client di cui completare il login
 
  memset(testa->username,0,CLIENT_NAME_LENGTH);

  int size = strlen(buff);
  char * temp = (char *) malloc(size);
  char * prec = (char *) malloc(size);
  char * pprec = (char *) malloc(size);
  strcpy(temp,buff);
  strtok(temp," ");
  int count = 0;
  

	while(temp != NULL) {
		if(count == 0) {
      strcpy(prec,temp);
    } else {
      strcpy(pprec,prec);
      strcpy(prec,temp);
    }
    	temp = strtok(NULL," ");
      count++;
	}
  
  while((testa!= NULL) && (testa->sock != fd))
      testa = testa->pun;

  memset(testa->username,0,CLIENT_NAME_LENGTH);
  strcpy(testa->username,pprec);
  
  memset(testa->udpPort,0,UDP_PORT_LENGTH);
  strcpy(testa->udpPort,prec);

  //free(pprec);
  //free(prec);

  return;    
}

struct Client* insertClient(struct Client* testa, char* buff, char* addr, int sk) { 

  //Creo nuovo elemento di tipo Client 

  struct Client * elem = (struct Client*)malloc(sizeof(struct Client));
  memset(elem, 0 ,sizeof(elem));

  elem->state = 1;  //all'inizio il client è libero
  elem->sock = sk;
  strcpy(elem->ipAddress, addr);
  if(testa == NULL) //inserimento del primo elemento
    elem->pun = NULL;
  else
    elem->pun = testa;

  testa = elem;
  
  completeLogClient(testa, elem->sock, buff); 
  
  printf("Connessione stabilita con il client \n");
  printf("%s si e' connesso \n",elem->username);
  printf("%s e' libero [L] \n",elem->username);

  return testa;
 
}

struct Client* disconnectClient(struct Client *testa, char *buff, int sk, int* numeroClient, fd_set read_fd) {
  struct Client* elem = testa;
  char  opponent_name[CLIENT_NAME_LENGTH];
  int ret;

  while((elem != NULL) && (elem->sock != sk))
    elem = elem->pun;
  

    printf("%s si e' disconnesso da %s\n", elem->username, elem->opponent);
    elem->state = 1;
    printf("%s e' libero [L]\n",elem->username);
    strcpy(opponent_name,elem->opponent);
    
    elem = testa;
    while(strcmp(elem->username,opponent_name) != 0)
      elem = elem->pun;

    elem->state = 1;							
    printf("%s e' libero [L]\n",elem->username);

    memset(buff,0,DIM_TCP_MSG);
    strcpy(buff,"!disconnect");						//Avversario si è arreso, quindi invio messaggio che notifica la vittoria

    ret = sendTcpMessage(elem->sock, buff);
    
    if(ret <= 0)
    {
      removeClient(testa,elem->sock);
      	if(*numeroClient == 1)
        		testa = NULL;
      (*numeroClient)--;
      close(sk);
      FD_CLR(sk,&read_fd);
    }
    return testa;
}

int whichOperation(char * buff) {

  if(strcmp(buff,"!who") == 0)
    return WHO;
  if(strcmp(buff,"!disconnect") == 0)
    return DISCONNECT;
  if(strcmp(buff,"!quit") == 0)
    return QUIT;

  char * retNo, *retYes,*retLogin, *retConnect;
  retNo = strstr(buff,"NO");
  retYes = strstr(buff,"YES");
  retLogin = strstr(buff,"login");
  retConnect = strstr(buff,"connect");
  if(retNo != NULL)
    return NO;
  if(retYes!= NULL)
    return YES;
  if(retLogin != NULL)
    return LOGIN;
  if(retConnect != NULL)
    return CONNECT;

    return 0;
}

int checkDuplicate(struct Client* testa, char* buff) {

  char nome[CLIENT_NAME_LENGTH];

  int size = strlen(buff);
  char * temp = (char *) malloc(size);
  strcpy(temp,buff);
  strtok(temp," ");
  int count = 0;

	while(temp != NULL && count != 1 ) {
		count++;
    temp = strtok(NULL," ");
	}
  memset(nome, 0, CLIENT_NAME_LENGTH);
  strcpy(nome,temp);

  while( (testa!= NULL) && (strcmp(testa->username,nome) != 0) )
    testa = testa->pun;
    
  if(testa == NULL)
    return 0;

  return -1;

  //free(temp);
}

int inizioPartita(struct Client* testa, int sk,char* buff) {

  struct Client *elem;
  char nomeClientAcceptedMatch[CLIENT_NAME_LENGTH];         //nome client che ha accettato la richiesta di inziare una partita
  char opponentName[CLIENT_NAME_LENGTH];                    //variabile di appoggio per memorizzare nome client che ha richiesto la partita

  elem = testa;
  while((elem != NULL) && (elem->sock != sk))
    elem = elem->pun;

  elem->state = 0;                        //Imposto ad occupato il client che ha ricevuto richiesta di inziare una nuova partita

  int size = strlen(buff);                //buff contiene nome client che ha richiesto la partita
  char * temp = (char *) malloc(size);
  char * prec = (char *) malloc(size);
  strcpy(temp,buff);
  strtok(temp," ");

  while(temp != NULL ) {
      prec = temp;
      temp = strtok(NULL," ");
  }
  
  memset(elem->opponent,0,CLIENT_NAME_LENGTH);
  strcpy(elem->opponent,prec);                  //memorizzo nome client che ha rich partita nel campo opponent del client che ha ricevuto rich di inizio partita
  memset(opponentName,0,CLIENT_NAME_LENGTH);
  strcpy(opponentName,elem->opponent);
                                                  //invio indirizzo ip e porta udp del client che ha richiesto la partita
  memset(buff,0,DIM_TCP_MSG);
  strcpy(buff,"YES ");
  strcat(buff,elem->opponent);
  strcat(buff," ");

  elem = testa;
  while((elem != NULL) && (strcmp(elem->username,opponentName)))
    elem = elem->pun;

  strcat(buff,elem->ipAddress); //Indirizzo Ip client che ha richiesto di iniziare una nuova partita
  strcat(buff," ");
  strcat(buff,elem->udpPort); //Porta UDP client che ha richiesto di iniziare una nuova partita
  
  int j = sendTcpMessage(sk,buff);

  if(j <= 0)
    return j; 

  elem = testa;
  while((elem != NULL) && (elem->sock != sk))
    elem = elem->pun;   
                                                                //Preparo Ip Address e UDP Port del client che ha accettato la partita
  memset(buff,0,DIM_TCP_MSG);
  strcpy(buff,"YES ");
  strcat(buff,elem->username);
  strcat(buff," ");
  strcat(buff,elem->ipAddress);
  strcat(buff," ");
  strcat(buff,elem->udpPort);

  strcpy(nomeClientAcceptedMatch,elem->username);
  memset(opponentName,0,CLIENT_NAME_LENGTH);
  strcpy(opponentName,elem->opponent);

  elem = testa;
  while((elem != NULL) && (strcmp(elem->username,opponentName) != 0))
    elem = elem->pun;

  elem->state = 0;                                   //imposto ad occupato anche il client che ha richiesto di inizare una partita
  strcpy(elem->opponent,nomeClientAcceptedMatch);    //inizializzo il campo opponent del client che ha richiesto partita col nome dell'avversario

  j = elem->sock;                                    //ritorno descrittore del client che ha richiesto la partita

  printf("%s si e' connesso a %s\n",elem->username,nomeClientAcceptedMatch);   //client che ha richiesto partita si è connesso col client con cui voleva giocare

  return j;
}

void elencoClientConnessi(struct Client* testa, int numero,int sk, char* buff) {

  struct Client* elem;
  
  memset(buff, 0, DIM_TCP_MSG);
  strcpy(buff,"WHO ");

    if(numero == 1) {
      strcat(buff,"Attualmente, sei l'unico cliente collegato.\n");
      return;
    }
    else if(testa == NULL) {
      strcat(buff,"Non ci sono altri utenti collegati.\n");
      return;
    }

  elem = testa;
  strcat(buff,"Client connessi al server: ");
  
  while(elem != NULL) {
    if(elem->sock != sk) {
     	 strcat(buff,elem->username);
      	if(elem->state == 1)
			strcat(buff,"[L] ");
      	else
			strcat(buff,"[O] ");
    }
      elem = elem->pun;
  }
  strcat(buff,"\n");
  
  return;
}

int connectClient(struct Client* testa, char* buff, int sk) {

  struct Client* elem;
  struct Client* temporanean;
  char nome[CLIENT_NAME_LENGTH];

  int size = strlen(buff);
  char * temp = (char *) malloc(size);
  char * prec = (char *) malloc(size);
  strcpy(temp,buff);
  strtok(temp," ");

	while(temp != NULL) {
      prec = temp;
    	temp = strtok(NULL," ");
	}

  strcpy(nome,prec);

  memset(buff,0,DIM_TCP_MSG);
  temporanean = testa;
  elem = testa;
  while((elem != NULL) && (strcmp(elem->username,nome)!= 0))
    elem = elem->pun;

  if(elem == NULL)
  {	
    strcpy(buff,"NO Impossibile connettersi a ");
    strcat(buff,nome);
    strcat(buff,": utente inesistente\n");
    return 0;
  }
  if(elem->sock == sk)
  {
    strcpy(buff,"NO Impossibile stabilire una partita con te stesso.Riprova...\n");
    return 0;
  }
  if(elem->state == 0)
  {	
    strcpy(buff,"NO Impossibile connettersi a ");
    strcat(buff,nome);
    strcat(buff,": utente impegnato in un'altra partita.Riprova...\n");
    return 0;
  }
  else
  {	
    strcpy(buff,"PARTITA ");

    while( (temporanean != NULL) && (temporanean->sock != sk) )
    temporanean = temporanean->pun;

    elem->state = 0;
    temporanean->state = 0;										//setto ad occupato entrambi i client.
    strcat(buff,temporanean->username);        //invio nome client che ha richiesto la partita
    return elem->sock;									//ritorno tcp socket del client a cui inviare la richiesta di partita
  }

}  

int main(int argc, char* argv[]) {

	fd_set interestedReadDescriptors; //Descrittori di lettura a cui sono interessato
	fd_set readDescriptors;			  //Descrittori di lettura modificati dalla select()
	int maxControlledDescriptors;	  //Numero max di descrittori da controllare
	int listeningSocket; 			  //Socket su cui il server si mette in ascolto
	int connectionSocket;			  //Socket relativa alla connesione stabilita
	struct sockaddr_in serverAddr;    //Struttura dati per memorizzare Indirizzo Server (ip_addr + port)
	struct sockaddr_in clientAddr;	  //Struttura dati per memorizzare Indirizzo Client (ip_addr + port)
	int clientAddrLen;				  //Dimensione della struttura che contiene indirizzo client
	int yes;						  //REUSE_ADDR in setsockopt richiede un parametro intero per optval
	int nbytes, i, j;				  //caratteri letti correttamente dal buffer di ingresso associato alla socket, variabili per scorrere des
	char buffer [DIM_TCP_MSG];		  //Buffer di memoria per memorizzare i dati passati dal client
	char indirizzoServer[16];		  //Salvtaggio indirizzo causa sua modifica da parte della funzione validateAddress()
	struct Client * p_client = NULL;  //Puntatore alla testa della lista dei client connessi
	int numClient = 0;					  //Numero di Client connessi = numero elementi nella lista puntata da p_client	
	int operation;					  //Tipo di operazione richiesta

	FD_ZERO(&interestedReadDescriptors);		//Inizializzazione a zero di tutti i descrittori
	FD_ZERO(&readDescriptors);

	

	if(argc == 3) {
    strcpy(indirizzoServer,argv[1]);
		if( validateAddress(argv[1],strlen(argv[1])) == -1 ) {
			printf("Server address is not valid. Please try again!\n\n");
			exit(1);
		}

			if( atoi(argv[2]) >= 65536) {
      				printf("Server port is not valid. Please try again!\n\n");
      				exit(1);
   			}

	} else {
		printf("Parameter's number is not valid.\nPlease use the following pattern: ./forza_server <host> <port>\n\n");
		exit(1);
	}
	

	printf("\nIndirizzo: %s (Porta: %s)\n",indirizzoServer,argv[2]);

	//Creazione del Server Socket
	if( (listeningSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {					
			perror("A problem occured while creating Server Socket!\n\n");
			exit(1);
	}

	//Settaggio opzioni associate con la socket
	if(setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("Server Address already in use!\n\n"); 
		close(listeningSocket);
		exit(1);
	}

	memset(&serverAddr,0,sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(atoi(argv[2]));
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if( bind(listeningSocket,(struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
			perror("Error while executing bind():"); 
			close(listeningSocket);
			exit(1);
	}

	if( listen(listeningSocket,10) <0) {
		perror("Error while executing listen()!\n\n"); 
		close(listeningSocket);
		exit(1);
	}

	FD_SET(listeningSocket,&interestedReadDescriptors);
	maxControlledDescriptors = listeningSocket;

	for(;;) {

		readDescriptors = interestedReadDescriptors;
    memset(buffer, 0, DIM_TCP_MSG);

		if(select(maxControlledDescriptors+1, &readDescriptors, NULL, NULL, NULL) < 0) {
			perror("Error while executing select()!\n\n"); 
			close(listeningSocket);
			exit(1);
		}

		for(i = 0; i <= maxControlledDescriptors; i++) {

				if(FD_ISSET(i,&readDescriptors)) {

					if (i == listeningSocket) {							//Arrivo nuova connesione

						memset(&clientAddr, 0, sizeof(clientAddr));
						clientAddrLen = sizeof(clientAddr);

						if( (connectionSocket = accept(listeningSocket,(struct sockaddr *)&clientAddr,(socklen_t *)&clientAddrLen)) < 0 ) {
							perror("Error while executing connect()!\n\n"); 
							close(listeningSocket);
							exit(1);
						}

						//Memorizzazione indirizzo ip del client che si è connesso
						 connectedClient[connectionSocket] = malloc(IP_SIZE);
    					 memset(connectedClient[connectionSocket], 0, IP_SIZE);
    					 inet_ntop(AF_INET, &(clientAddr.sin_addr), connectedClient[connectionSocket], INET_ADDRSTRLEN);

    					 FD_SET(connectionSocket,&interestedReadDescriptors);
    					 if(connectionSocket > maxControlledDescriptors)
    					 	maxControlledDescriptors = connectionSocket;

					}else {

						if( (nbytes = recvTcpMessage(i,buffer)) <= 0) {     //Quanti byte il Server deve leggere dal buffer di ingresso
							perror("Terminazione imprevista. Hang up da parte del client!");

								if(numClient > 0) {	

									if(isLogged(p_client,i) == 1) {

            							if( clientState(p_client,i) == 0)				  //Verifico se è client impegnato in una partita
          										p_client = disconnectClient(p_client,buffer,i,&numClient,readDescriptors);

		    									 p_client = removeClient(p_client,i) ;
	        									
	        									 if(numClient == 1)
              											p_client = NULL;

                              numClient--;
		   				    		}
		   				    	}

		   				    	close(i);
	    						  FD_CLR(i,&interestedReadDescriptors);
		   				    	break;
						}

						if( (operation = whichOperation(buffer)) < 0)						//buffer contiene tipo operazione richiesta
	    						printf("Error while executing whichOperation()!\n\n");

	    				switch(operation) {					//In base al tipo di operazione richiesta, faccio elaborazioni e invio msg al client
	    						
	    						case LOGIN:	//Login Client sul Server

                      
	    									if(checkDuplicate(p_client, buffer) < 0) {		//Presente utente con lo stesso username
														printf("Specificato nome utente gia' presente! REIMMISSIONE\n");

    										     memset(buffer, 0, sizeof(buffer));
											       strcpy(buffer,"NUOVO_NOME");

											if( (nbytes = sendTcpMessage(i,buffer)) <= 0) {
		 	 									if(nbytes < 0) {
		    										printf("Error while trying to get new client username.\n\n");
		    										close(i);
		    										FD_CLR(i,&interestedReadDescriptors);
		    									} else {
		    										close(i);
		       										FD_CLR(i,&interestedReadDescriptors);
		    									}
		    								}
		    									break;
	    									}else {

	    										p_client = insertClient(p_client, buffer, connectedClient[i],i);
												  numClient++;
    											memset(buffer, 0, sizeof(buffer));
    											strcpy(buffer,"OK");
    											if((nbytes = sendTcpMessage(i,buffer)) <= 0) {
                                perror(NULL);
      													p_client = removeClient(p_client,i);
      													numClient--;
      													if(numClient == 1)
       			   												p_client = NULL;
     													close(i);
      													FD_CLR(i,&interestedReadDescriptors);
    											}
    										break; 	
	    									}


	    						case WHO:										//Richiesta client connessi
	    									memset(buffer,0,DIM_TCP_MSG);
	      									elencoClientConnessi(p_client,numClient,i,buffer);
	      									nbytes = sendTcpMessage(i,buffer);
	      									if( nbytes <= 0) {
												p_client = removeClient(p_client,i);
												if(numClient == 1)
		  	  										p_client = NULL;
												numClient-- ;
												close(i);
												FD_CLR(i, &interestedReadDescriptors);
	      									}
	     									 break;

	     						case CONNECT:

	     									j = connectClient(p_client,buffer,i);				//j contiene tcp socket del client a cui inviare richiesta di partita
          									if(j == 0) { 										//error su client desiderato per match 
            									if((nbytes = sendTcpMessage(i,buffer)) <= 0) {
              										p_client = removeClient(p_client ,i);
             										 if(numClient == 1)
                									  		p_client = NULL;
              										  numClient-- ;
              										  close(i);
              										  FD_CLR(i,&interestedReadDescriptors);
            									}
          									} else { 
          										if((nbytes = sendTcpMessage(j,buffer)) <= 0) {
             										p_client = removeClient(p_client ,i);
              										if(numClient == 1)
                											p_client = NULL;
             										numClient-- ;
              										close(i);
              										FD_CLR(i,&interestedReadDescriptors);
            									} 
          									}
	     									 break;

	     						case DISCONNECT: 
	     										 p_client = disconnectClient(p_client,buffer,i,&numClient,readDescriptors);
	      										 break;

	      						case QUIT:
	      										printf("Richiesta di disconnessione\n");
          										if( clientState(p_client,i) == 0)  			//Client impegnato in una partita
            											p_client = disconnectClient(p_client,buffer,i,&numClient,readDescriptors);

          										p_client = removeClient(p_client,i) ;
          										strcpy(buffer,"QUIT");
          										sendTcpMessage(i,buffer);
	      
	      										if(numClient == 1)
		   											 p_client = NULL;

	      										numClient--;
        
          										close(i);
          										FD_CLR(i,&interestedReadDescriptors);
	      										break;

	      						case NO:														                   //Client ha rifiutato la richiesta di avviare una nuova partita
	      										j = getClientAgainFree(p_client,i,buffer);		//j contiene tcp socket del client da cui è partita la richiesta di una nuova partita
          										nbytes = sendTcpMessage(j,buffer);
          										if(nbytes <= 0) {
            										p_client = removeClient(p_client ,i);
            										if(numClient == 1)
              												p_client = NULL;
            										numClient-- ;
            										close(i);
            										FD_CLR(i,&interestedReadDescriptors);
          										}
          										break;

                    case YES:
                              j = inizioPartita(p_client,i,buffer);
                              if(j <= 0) {
                                    p_client = disconnectClient(p_client,buffer,i,&numClient,readDescriptors);
                                    p_client = removeClient(p_client,i);
                                    if(numClient == 1)
                                          p_client = NULL;
                                    numClient-- ;
                                    close(i);
                                    FD_CLR(i,&interestedReadDescriptors);
                              }else {
                                  nbytes = sendTcpMessage(j,buffer);
                                  if(nbytes == 0) {
                                        if(clientState(p_client,j) == 0)
                                            p_client = disconnectClient(p_client,buffer,j,&numClient,readDescriptors);
                                        p_client = removeClient(p_client,j);
                                        if(numClient == 1)
                                              p_client = NULL;
                                        numClient-- ;
                                        close(j);
                                        FD_CLR(j,&interestedReadDescriptors);
                                  }
                              }
                                break;

	    						default:
		 								         printf("Command not found!\n\n");
		  								       break;						
						}

				    }//fine ELSE
            
			    }//chiusura dell'IF(FD_ISSET)
    
    		} //chiusura for(i; i< fdmax..)
    
  		}//chiusura for(;;) 

return 0;

}