#include <stdlib.h>
#include <stdio.h>																							
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>

#define DIM_TCP_MSG 1024
#define CLIENT_NAME_LENGTH 22
#define UDP_PORT_LENGTH 7
#define IP_SIZE 16
#define COMMAND_LENGTH 30
#define DIM_UDP_MSG 256
#define MATCH_STARTED 1  
#define START 10
#define SC ">  "
#define SP "#"
#define STDIN 0


char clientName[CLIENT_NAME_LENGTH]; 
char udpPort[UDP_PORT_LENGTH];
int  stateMatch = 0;											//se libero oppure occupato
int  turno = 0;											      //inserimento gettone solo quando è il proprio turno
int  tcpSocket;
char tcpBuffer[DIM_TCP_MSG];

char opponentName[CLIENT_NAME_LENGTH];
char opponentIpAddress[IP_SIZE];
char opponentUdpPort[UDP_PORT_LENGTH]; 

struct sockaddr_in opponentPeer;					

char actual;                             //gettone per il turno corrente
char map [6][7];                        //mappa di gioco

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

int sendUdpMessage(int sk,char* buff) {

  int ret;
  ret = write(sk,buff,strlen(buff));

  if(ret != strlen(buff))
  {
    printf("Errore durante l'invio dei datagrammi UDP\n");
    return -1;
  }
  else
    return ret;
}

int rcvUdpMessage(int sk, char*buff) {

  int ret;
  int sizeCmdWin, sizeCmdTimeout, sizeCmdInsert, sizeCmdAnswer;

  sizeCmdWin = strlen("HAI VINTO");
  sizeCmdTimeout = strlen("TIMEOUT");
  sizeCmdInsert = strlen("!insert  ");                            //[!insert a]
  sizeCmdAnswer = strlen("RISPOSTA");                             //[RISPOSTA] 
  

  memset(buff,0,DIM_UDP_MSG);

  ret = read(sk,buff,DIM_UDP_MSG);

  if(ret < 0)
  {
     printf("Errore durante la ricezione dei datagrammi UDP\n");
    return -1;
  }

  if(ret == 0)
    return 0;

  if( ret == sizeCmdWin)
        return ret;
  if(ret == sizeCmdTimeout)
        return ret;
  if(ret == sizeCmdInsert)
        return ret;
  if(ret == sizeCmdAnswer)
    return ret;
  else {
        printf("Errore durante la ricezione dei datagrammi.Ricevuta solo una parte del messaggio UDP\n");
        return -1;
  }
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

void help(int i) {
  if(i == 0){
    printf("\nSono disponibili i seguenti comandi:\n");
    printf(" * !help  -->  mostra l'elenco dei comandi disponibili\n");
    printf(" * !who  -->  mostra l'elenco dei client connessi al server\n");
    printf(" * !connect nome_client  -->  avvia una partita con l'utente nome_client\n");
    printf(" * !disconnect  -->  disconnette il client dell'attuale partita intrapresa con un altro peer\n");
    printf(" * !quit  --> disconnette il client dal server\n");
    printf(" * !show_map  -->  mostra la mappa di gioco\n");
    printf(" * !insert column  --> inserisce il gettone in column (valido solo quando è il proprio turno\n");
    printf("\n");
  } else {
    printf("\nSono disponibili i seguenti comandi:\n");
    printf(" * !help  -->  mostra l'elenco dei comandi disponibili\n");
    printf(" * !who  -->  mostra l'elenco dei client connessi al server\n");
    printf(" * !connect nome_client  -->  avvia una partita con l'utente nome_client\n");
    printf(" * !disconnect  -->  disconnette il client dell'attuale partita intrapresa con un altro peer\n");
    printf(" * !quit  --> disconnette il client dal server\n");
    printf(" * !show_map  -->  mostra la mappa di gioco\n");
    printf(" * !insert column  --> inserisce il gettone in column (valido solo quando è il proprio turno\n");
    printf("\n");
    if( MATCH_STARTED == stateMatch )                                   //Partita avviata
    printf("# ");
    else
    printf("> "); 
    fflush(stdout); 
  }
  
  return;
}

void immettiPorta(char* porta_udp) {

  printf("Inserisci la porta UDP di ascolto: ");
  memset(porta_udp, 0, UDP_PORT_LENGTH);
  fgets(porta_udp, UDP_PORT_LENGTH, stdin);

  while((atoi(porta_udp) >= 65536) || (atoi(porta_udp) <= 1023) || ((strlen(porta_udp) == UDP_PORT_LENGTH-1)  && (porta_udp[strlen(porta_udp)-1] != '\n')) ) //controllo valore porta UDP digitato
  {
    printf("Numero porta UDP di ascolto non valido! Premere un tasto per continuare...");
    while(getchar()!='\n');                                   
    printf("Inserisci la porta UDP di ascolto: ");
    memset(porta_udp, 0, UDP_PORT_LENGTH);
    fgets(porta_udp, UDP_PORT_LENGTH, stdin);

  }
    porta_udp[strlen(porta_udp)-1] = '\0';
  
  return;
}

void login(int sk_tcp,char* nome_client, char* porta_udp, char* buffer_tcp) {

  int ret;
  printf("Inserisci il tuo nome: ");
  memset(nome_client, 0, CLIENT_NAME_LENGTH);
  fgets(nome_client, CLIENT_NAME_LENGTH, stdin);    //LEGGO DALLO STDIN e memorizzo il clientName.Max client_name_length char oppure \n o EOF

  while(nome_client[0] == '\n' || ((strlen(nome_client) == CLIENT_NAME_LENGTH-1) && (nome_client[strlen(nome_client)-1] != '\n'))) {

    printf("Nome inserito non valido.Riprova!\n");
    while(getchar()!='\n');   
    printf("Inserisci il tuo nome: ");
    memset(nome_client, 0, CLIENT_NAME_LENGTH);
    fgets(nome_client, CLIENT_NAME_LENGTH, stdin); 
  }
  nome_client[strlen(nome_client)-1] = '\0';
  immettiPorta(porta_udp);
 
  //INVIO LOGIN AL SERVER (USERNAME,PORTA_UDP)
  memset(buffer_tcp,0,DIM_TCP_MSG);
  strcpy(buffer_tcp,"login ");
  strcat(buffer_tcp,nome_client);
  strcat(buffer_tcp," ");
  strcat(buffer_tcp,porta_udp);
  
 if((ret = sendTcpMessage(sk_tcp,buffer_tcp)) <= 0)
 {
    perror(NULL);
    close(sk_tcp);
    exit(1);
 } 
}

int loginValidation() {

  if(recvTcpMessage(tcpSocket,tcpBuffer) <= 0)
    return -1;

  if(strcmp(tcpBuffer,"NUOVO_NOME") == 0)
  {
    printf("Il nome utente specificato risulta gia' utilizzato!Riprova...\n");
    login(tcpSocket, clientName, udpPort, tcpBuffer); //invia nome utente e porta udp al server
    loginValidation();
    return 1;
  }
  if(strcmp(tcpBuffer,"OK") == 0)
  {
    printf("Autenticazione client completata con successo!\n");
    return 1;
  }

  return 0;
}

void askForAMatch(char* buff) {

  memset(buff,0,DIM_TCP_MSG);

  fgets(buff,DIM_TCP_MSG,stdin);

  while( (buff[0] == '\0') || (buff[0] =='\n') ) {
    printf("Valore digitato non valido!");
    printf("Inserire y[yes] oppure n[no]: ");
    memset(buff,0,DIM_TCP_MSG);
    fgets(buff,DIM_TCP_MSG,stdin);
  }

  while( ((strlen(buff) > 2) && (buff[1] != '\n')) || (buff[0] == '\0') || (buff[0] =='\n')) {
    printf("Valore digitato non valido! Premere un tasto per continuare...");
    while(getchar() != '\n');
    printf("Inserire y[yes] oppure n[no]: ");
    memset(buff,0,DIM_TCP_MSG);
    fgets(buff,DIM_TCP_MSG,stdin); 
  }
  buff[strlen(buff)-1] = '\0';

  stateMatch = MATCH_STARTED;                 //Imposto stateMatch = partita avviata
  turno = 0;                                  //Turno = 0 (gioca prima il client che ha inviato richiesta di partita)

  if((strcmp(buff,"y") == 0)) {

    printf("\n\t\tHAI ACCETTATO L'INVITO AD INIZIARE UNA NUOVA PARTITA!\n");
    memset(buff,0,DIM_TCP_MSG);
    strcpy(buff,"YES ");
    strcat(buff,opponentName);
    return;
  }

  if((strcmp(buff,"n") == 0)) {

    printf("\n\t\tHAI RIFIUTATO L'INVITO AD INIZIARE UNA NUOVA PARTITA!\n");
    printf("> ");
    fflush(stdout);
    strcpy(buff,"NO ");
    strcat(buff,opponentName);
    memset(opponentName,0,CLIENT_NAME_LENGTH);
    stateMatch = 0;
    return;
  }
  printf("Valore inserito non valido!Inserire y[yes] oppure n[no]: ");
  askForAMatch(buff);
}

void showMap() {
  int i, j;

  for(i = 0; i < 6; i++){
    printf("\n");
    for(j = 0; j < 7; j++)
        printf("%c\t",map[i][j]);
  }
  printf("\n");
  if(turno == 1){
    printf("# ");
    fflush(stdout);
  }
    
  return;
}

int forzaQuattroObtained(char *buff) {

      showMap();
      printf("\n\t\t%s ha occupato 4 caselle consecutivamente!\n",opponentName);
      printf("\t\t!!!!!!!!!!!!!!! HAI PERSO !!!!!!!!!!!!!!! \n");
      memset(buff,0,DIM_UDP_MSG);
      sprintf(buff,"HAI VINTO");
      printf("> ");
      fflush(stdout);  
      return 0;
}

int forzaQuattroNotObtained(char * col, int row, char *buff) {

  int riga;
  if(row == 5)
      riga = 1;
  else if( row == 4)
      riga = 2;
  else if( row == 3)
      riga = 3;
  else if( row == 2)
      riga = 4;
  else if( row == 1)
      riga = 5;
  else 
      riga = 6;

  
      printf("%s ha marcato la casella %s%d\n",opponentName,col,riga);
      printf("# ");
      fflush(stdout);
      memset(buff,0,DIM_UDP_MSG);
      sprintf(buff,"RISPOSTA");
      return 1;
}

int colonControl(int row, int j){
  int n;
  int giusti = 0;

  if(j == 0){

    for(n = 3; (n >= 0) && (map[row][n] == 'O'); n--)
            giusti++;

      if( giusti == 4)
          return 1;
      else
          return 0;

  } else if( j==1 ) {

    for(n = 3; (n >= 0) && (map[row][n] == 'O'); n--)
            giusti++;

    if(giusti != 4){
      giusti = 0;

      for(n = 4; (n >= 1) && (map[row][n] == 'O'); n--)
            giusti++;

    }

      if( giusti == 4)
          return 1;
      else
          return 0;

  } else if( j==2 ) {

    for(n = 3; (n >= 0) && (map[row][n] == 'O'); n--)
            giusti++;

    if(giusti != 4){

      giusti = 0;

      for(n = 4; (n >= 1) && (map[row][n] == 'O'); n--)
            giusti++;

      if(giusti != 4){

        giusti = 0;

        for(n = 5; (n >= 2) && (map[row][n] == 'O'); n--)
            giusti++;

      }

    }
    
    if( giusti == 4)
          return 1;
      else
          return 0;

  } else if( j==3 ) {

    for(n = 3; (n >= 0) && (map[row][n] == 'O'); n--)
            giusti++;

    if(giusti != 4){

      giusti = 0;

      for(n = 4; (n >= 1) && (map[row][n] == 'O'); n--)
            giusti++;

      if(giusti != 4){

        giusti = 0;

        for(n = 5; (n >= 2) && (map[row][n] == 'O'); n--)
            giusti++;

        if(giusti != 4) {

          giusti = 0;

          for(n = 6; (n >= 3) && (map[row][n] == 'O'); n--)
            giusti++;

        }

      }

    }
    
    if( giusti == 4)
          return 1;
      else
          return 0;

  } else if( j==4 ) {

    for(n = 4; (n >= 1) && (map[row][n] == 'O'); n--)
            giusti++;

    if(giusti != 4){

      giusti = 0;

      for(n = 5; (n >= 2) && (map[row][n] == 'O'); n--)
            giusti++;

      if(giusti != 4){

        giusti = 0;

        for(n = 6; (n >= 3) && (map[row][n] == 'O'); n--)
            giusti++;

      }

    }
    
    if( giusti == 4)
          return 1;
      else
          return 0;

  } else if( j == 5) {

    for(n = 6; (n >= 3) && (map[row][n] == 'O'); n--)
            giusti++;

      if( giusti != 4){
          giusti = 0;

          for(n = 5; (n >= 2) && (map[row][n] == 'O'); n--)
            giusti++;

      }

      if( giusti == 4)
          return 1;
      else
          return 0;

  } else {

    for(n = 6; (n >= 3) && (map[row][n] == 'O'); n--)
            giusti++;

      if( giusti == 4)
          return 1;
      else
          return 0;
  }
}

int rowsControl(int row, int j){
  int m;
  int giusti = 0;

  if( row == 2){

    for(m = 5; (m >=2 ) && (map[m][j] == 'O'); m--)
            giusti++;

    if( giusti == 4)
          return 1;
      else
          return 0;

  } else if(row == 1){

    for(m = 4; (m >=1 ) && (map[m][j] == 'O'); m--)
            giusti++;

    if( giusti == 4)
          return 1;
      else
          return 0;

  } else if(row == 0) {

    for(m = 3; (m >=0 ) && (map[m][j] == 'O'); m--)
            giusti++;

    if( giusti == 4)
          return 1;
      else
          return 0;
  } else
      return 0;
    
}

int diagSecControl(int row, int j) {

  int giusti = 0;
  int m, n;

  if(row == 2) {

    if( j == 3) {
        n = j;

        for(m = 2; (m <6 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n--;
        }

        if( giusti == 4)
          return 1;
        else
          return 0; 

    } else if( j == 4 ) {
        n = j;

        for(m = 2; (m <6 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n--;
        }

        if( giusti == 4)
          return 1;
        else
          return 0; 

    } else if( j == 5 ) {
        n = j;

        for(m = 2; (m <6 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n--;
        }
        
        if( giusti == 4)
          return 1;
        else
          return 0; 

    } else {
      n = j;

        for(m = 2; (m <6 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n--;
        }
        
        if( giusti == 4)
          return 1;
        else
          return 0;  
    }

  } else if(row == 1) {

    if( j == 4) {
        n = j;

        for(m = 1; (m <5 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n--;
        }

        if( giusti == 4)
          return 1;
        else
          return 0; 

    } else if( j == 5 ) {
        n = j;

        for(m = 1; (m <5 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n--;
        }

        if( giusti == 4)
          return 1;
        else
          return 0;     

    } else  {
        n = j;

        for(m = 1; (m <5 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n--;
        }
        
        if( giusti == 4)
          return 1;
        else
          return 0; 
    } 

  } else if (row == 0) {

    if( j == 5) {
        n = j;

        for(m = 0; (m <4 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n--;
        }

        if( giusti == 4)
          return 1;
        else
          return 0; 

    } else {
        n = j;

        for(m = 0; (m <4 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n--;
        }

        if( giusti == 4)
          return 1;
        else
          return 0;     

    } 
  } 
  else
      return 0;
}

int diagPrinControl (int row, int j) {
    
  int giusti = 0;
  int m, n;

  if(row == 2){

    if( j == 3) {
        n = j;

        for(m = 2; (m <6 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n++;
        }

        if( giusti == 4)
          return 1;
        else
          return 0; 

    } else if( j == 2 ) {
        n = j;

        for(m = 2; (m <6 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n++;
        }

        if( giusti == 4)
          return 1;
        else
          return 0;  

    } else if( j == 1 ) {
        n = j;

        for(m = 2; (m <6 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n++;
        }
        
        if( giusti == 4)
          return 1;
        else
          return 0; 

    } else {
      n = j;

        for(m = 2; (m <6 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n++;
        }
        
        if( giusti == 4)
          return 1;
        else
          return 0;  
    }

  } else if(row == 1) {

    if( j == 2) {
        n = j;

        for(m = 1; (m <5 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n++;
        }

        if( giusti == 4)
          return 1;
        else
          return 0; 

    } else if( j == 1 ) {
        n = j;

        for(m = 1; (m <5 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n++;
        }

        if( giusti == 4)
          return 1;
        else
          return 0;     

    } else  {
        n = j;

        for(m = 1; (m <5 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n++;
        }
        
        if( giusti == 4)
          return 1;
        else
          return 0; 
    } 

  } else if (row == 0) {

    if( j == 1) {
        n = j;

        for(m = 0; (m <4 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n++;
        }

        if( giusti == 4)
          return 1;
        else
          return 0; 

    } else {
        n = j;

        for(m = 0; (m <4 ) && (map[m][n] == 'O'); m++) {
            giusti++;
            n++;
        }

        if( giusti == 4)
          return 1;
        else
          return 0;     

    } 
  } else
      return 0;
}

int checkState(int row, char * col,char *buff) {

  int j;
  int quantiColon, quantiRow,quantiDiagPrin,quantiDiagSeco;
  quantiColon = 0;
  quantiRow = 0;
  quantiDiagPrin = 0;
  quantiDiagSeco = 0;

  if(strcmp(col,"a")==0)
    j = 0;
  else if(strcmp(col,"b")==0)
    j = 1;
  else if(strcmp(col,"c")==0)
    j = 2;
  else if(strcmp(col,"d")==0)
    j = 3;
  else if(strcmp(col,"e")==0)
    j = 4;
  else if(strcmp(col,"f")==0)
    j = 5;
  else
    j = 6;

  //controllo colonne
  quantiColon = colonControl(row,j);
  //controllo righe
  quantiRow = rowsControl(row,j);
  //controllo diagonale secondaria
  if( j >= 3 && j<= 6)
    quantiDiagSeco = diagSecControl(row,j);
  //controllo diagonale principale
  if( j >= 0 && j<= 3)
    quantiDiagPrin = diagPrinControl(row,j);

  if(quantiColon == 1 || quantiRow == 1 || quantiDiagSeco == 1 || quantiDiagPrin == 1)
      return forzaQuattroObtained(buff);
  
  return forzaQuattroNotObtained(col,row,buff);


}

int insertCoin(char * col){
  int i;
  int j;

  int riga = 0;
  int count = 0;

  if(strcmp(col,"a")==0)
    j = 0;
  else if(strcmp(col,"b")==0)
    j = 1;
  else if(strcmp(col,"c")==0)
    j = 2;
  else if(strcmp(col,"d")==0)
    j = 3;
  else if(strcmp(col,"e")==0)
    j = 4;
  else if(strcmp(col,"f")==0)
    j = 5;
  else
    j = 6;
  
  for(i = 5; i >= 0; i--){
    if( (map[i][j] == '-')) {
        //printf("yes");
        if(turno == 1)
        map[i][j] = 'X';
        else
        map[i][j] = 'O';  
        riga = i;
        //printf("row: %d",riga);
        break;
    } else
      count++; 
  }
  //printf("\nquanti : %d",count);
  if( count == 6)         //colonna piena
      return -1;
  return riga;
}

int evaluateUdpOperation(char* buff) {

  if(strcmp(buff,"TIMEOUT") == 0) {
    turno = 0;
    stateMatch = 0;
    printf("\n\t\tTEMPO A DISPOSIZIONE DELL'AVVERSARIO TERMINATO.\n");
    printf("\t\t*************** VITTORIA ***************\n");
    printf("> "); 
    fflush(stdout); 
    return 0;
  }

  //int i;
  //for(i=0; (i < strlen(buff)) && (buff[i]!= ' ');i++);
  char * temp = (char *) malloc(COMMAND_LENGTH);
  char * prec = (char *) malloc(COMMAND_LENGTH);
  int ris;
  strcpy(temp,buff);
  strtok(temp," ");
  int count = 0;

  while(temp != NULL && count != 1 ) {
    strcpy(prec,temp);
    temp = strtok(NULL," ");
    count++;
  }

  if(strcmp(prec,"RISPOSTA") == 0) { //[RISPOSTA]
    turno = 0;
    printf("E' il turno di %s\n",opponentName);
    ris = 0;
  }

  int ret,riga;
  if(strcmp(prec,"!insert") == 0) {
    riga = insertCoin(temp);
    if(riga == -1){
      printf("%s ha effettuato una mossa non valida!\n",opponentName);
      turno = 1;
      return 1;
    }
    ret = checkState(riga,temp,buff);

    if(ret == 1) {
      turno = 1;
      //printf("E' il tuo turno:\n");
      ris = 1;
    }else { //HO PERSO!!! ----> invio "HAI VINTO"al client e "!disconnect " al server
      stateMatch = 0;
      turno = 0;
      ris = 1;
    }
  }
  return ris;
}

int evaluateOperation(char* buff) {

  int i,j;
  char * appoggio = (char *)malloc(DIM_TCP_MSG);
  strcpy(appoggio,buff);
  strtok(appoggio, " ");
  int count = 0;

  while(appoggio != NULL && count != 1) {
    count++;
    strtok(NULL," ");
  }

  for(i=0;  (buff[i] != ' ') && (i<strlen(buff)) ;i++); 
  i++;

  if( (strcmp(appoggio,"NO") == 0) || (strcmp(appoggio,"WHO") == 0) ) {          //MOSTRO WHO: elenco client connessi NO:causa per la quale è impossibile fare connect
    printf("%s",(buff+i));
    if( MATCH_STARTED == stateMatch )                                   //Partita avviata
    printf("# ");
    else
    printf("> "); 
    fflush(stdout); 
    return 1;
  }

  if( strcmp(appoggio,"PARTITA") == 0) {                                    //CONNECT avvenuta con successo. Richiesta di partita al client specificato 
    printf("\t\t=============== INVITO AD AVVIARE UNA PARTITA ===============\n\n");
    printf("%s vorrebbe iniziare una partita con te.\nAccetti l'invito?\n",buff+i);
    printf("Per confermare premi y[yes] oppure premi n[no]: ");
    memset(opponentName,0,CLIENT_NAME_LENGTH);
    strcpy(opponentName,buff+i);

    askForAMatch(buff);
    
    i = sendTcpMessage(tcpSocket,buff);
    return i;
  }
  
  if(strcmp(buff,"!disconnect") == 0) {
    if( stateMatch != MATCH_STARTED)                        //a partita non iniziata la disconnect non produce risultato
      return 1;

    printf("\t\t\t L'AVVERSARIO SI E' ARRESO\n");        // a partita in corso, vuol dire che l'avversario si è arreso prima della fine della partita
    printf("\n\t\t*************** V I T T O R I A *************** \n");
    stateMatch = 0;                                                //Re-inizializzo lo stato del match a 0 (in attesa che una partita inizi)

    printf("> "); 
    fflush(stdout); 
    return 1;
  }
                                                                
  if(strcmp(appoggio,"YES") == 0) {                            //riposta affermativa alla connect nuome_utente [YES Nome_Utente IP portaUDP] 
                                                          //prima spedisco all'avversario username ip e udpPort client richiedente e poi faccio il contrario
    
    for(j=i; buff[i] != ' '; i++);                             //scorro fino all'altro spazio
    memset(opponentName,0,CLIENT_NAME_LENGTH);  
    strncpy(opponentName,buff+j,i-j);
    //printf("Nome avversario: %s",opponentName);
    i++;
    for(j=i; buff[i] != ' '; i++);
    memset(opponentIpAddress,0,IP_SIZE);
    strncpy(opponentIpAddress,buff+j,i-j);
    //printf("IP avversario: %s",opponentIpAddress);
    i++;
    memset(opponentUdpPort,0,UDP_PORT_LENGTH);
    strcpy(opponentUdpPort,buff+i);
    //printf("UDP PORTA avversario: %s",opponentUdpPort);
      return START;                                                //valore per indicare l'inzio della partita

  }
  return 1;
}

int whichCommand(char* buff) {
  
  if(strcmp(buff,"!help") == 0) {
    if(strlen(buff) != strlen("!help"))
      return -1;
    else
      return 1;
  } 

  if(strcmp(buff,"!who") == 0) {
    if(strlen(buff) != strlen("!who"))
      return -1;
    else
      return 2;
  }

  if(strcmp(buff,"!disconnect") == 0) {

    if(stateMatch != MATCH_STARTED) {
      printf("Comando non valido. Non hai ancora avviato una partita!\n");
      return -1;
    }
    if(strlen(buff) != strlen("!disconnect"))
      return -1;
    else {
      stateMatch = 0;
      printf("Disconnessione avvenuta con successo: TI SEI ARRESO\n");
      printf("> "); 
      fflush(stdout); 
      return 2; 
    }
  }

  if(strcmp(buff,"!quit") == 0) {
    if(strlen(buff) != strlen("!quit"))
      return -1;
    else
    return 3;
  }

  if(strcmp(buff,"!show_map") == 0) {

    if(stateMatch != MATCH_STARTED) {
      printf("Comando non valido.Al momento non sei impegnato in una partita.\n");
      return -1;
    }
    if(strlen(buff) != strlen("!show_map"))
      return -1;
    else
    return 5;
  }

  int len;
  char * appoggio = (char *)malloc(COMMAND_LENGTH);
  char * prec = (char *)malloc(COMMAND_LENGTH);
  char * pprec = (char *) malloc(COMMAND_LENGTH);

  const char str2[] = " ";
  len = strcspn(buff, str2);

  if(strlen(buff) == len){
    if(strcmp(buff,"!connect") == 0) {
      if(strlen(buff) != strlen("!connect")+1){
        printf("Specificare nome_client a cui connettersi!\n");
        return -1;
      }
        
      if(stateMatch == MATCH_STARTED) {
        printf("Comando non valido. Al momento stai giocando una partita.\n!disconnect per abbandonare\n");
        return -1;
      }
    }
    if(strcmp(buff,"!insert") == 0) {
      if(strlen(buff) != strlen("!insert")+1){
        printf("Specificare colonna nella quale inserire gettone!\n");
        return -1;
      }
        
      if(stateMatch != MATCH_STARTED) {
        printf("Comando non valido.Al momento non sei impegnato in una partita.\n");
        return -1;
      }
      if(turno == 0) {
        printf("L'avversario sta completando la sua mossa! Attendi il tuo turno.\n");
        return -1;
      }
    }
  }else{
    strcpy(appoggio,buff);
    strtok(appoggio," ");
    int count = 0;

    while(appoggio != NULL) {
      if(count == 0) {
        strcpy(prec,appoggio);
      } else {
        strcpy(pprec,prec);
        strcpy(prec,appoggio);
      }
        appoggio = strtok(NULL," ");
        count++;
    }

    if(strcmp(prec,"!connect") == 0)
      return -1;   

    if(strcmp(pprec,"!connect") == 0){
      if(stateMatch == MATCH_STARTED) {
        printf("Commando non valido.Al momento stai giocando una partita.\n");
        return -1;
      }
      turno = 1;  
      return 2;
    }

    if(strcmp(prec,"!insert") == 0)
      return -1;   

    if(strcmp(pprec,"!insert") == 0) {

      if(strlen(prec) >= 2 || strlen(prec) == 0){
        printf("Colonna non valida! Specificare una colonna compresa tra i caratteri a e g!\n");
        return -1;
      }

      if(strcmp(prec,"a") < 0 || strcmp(prec,"g") > 0){
        printf("Colonna specificata non valida!\n");
        return -1;
      }
          

      if(stateMatch != MATCH_STARTED) {
        printf("Comando non valido.Al momento non sei impegnato in una partita.\n");
        return -1;
      }

      if(turno == 0) {
        printf("L'avversario sta completando la sua mossa! Attendi il tuo turno.\n");
        return -1;
      }

      return 4;
    }   
  } 

return -1;
    
}

void iniziaPartita() {
  
  stateMatch = MATCH_STARTED;

  int a,b;
  for(a = 0; a < 6; a++){
    for(b = 0; b < 7; b++)
        map[a][b] = '-';
  }

  if(turno == 1) {
    printf("%s ha accettato la partita\n",opponentName);
    printf("Partita avviata con %s\n",opponentName);
    printf("Il tuo simbolo e': X\n");
    printf("E' il tuo turno:\n");
    actual = 'X';
  }
  else {
    printf("\nPartita avviata con %s\n",opponentName);
    printf("Il tuo simbolo e': O\n");
    printf("E' il turno di %s\n",opponentName);
    actual = 'O';
  }
  return;
}

int max (int a, int b , int c) {

    if(a > b) {
        if(a > c)
            return a;
    }
    if( b > c)
      return b;

    return c;
}
int insertCoinIntoPersonalMap(char * cmd){

    char * appoggio = (char *)malloc(COMMAND_LENGTH);
    char * prec = (char *)malloc(COMMAND_LENGTH);
    char * pprec = (char *) malloc(COMMAND_LENGTH);
    strcpy(appoggio,cmd);
    strtok(appoggio," ");
    int count = 0;
    int riga;

    while(appoggio != NULL) {
      if(count == 0) {
        strcpy(prec,appoggio);
      } else {
        strcpy(pprec,prec);
        strcpy(prec,appoggio);
      }
        appoggio = strtok(NULL," ");
        count++;
    }
    riga = insertCoin(prec);
    if(riga == -1){
      printf("%s hai effettuato una !insert non valida!\nE' ancora il tuo turno:\n",clientName);
      return -1;
    }
    return 0;
}

int main(int argc, char * argv[]) {

  char *ipAddressServer;                          //conterra' <host_remoto> "127.0.0.1"
  unsigned short int portServer;                  //conterra' <porta_server> "1234"
  struct sockaddr_in server;  
  char indirizzoServer[16];                       //Salvataggio indirizzo causa sua modifica da parte della funzione validateAddress()

  struct sockaddr_in local; 
  int udpSocket;
  char udpBuffer[DIM_UDP_MSG];
  int i, ret, nbytes;
  int operation;                                  //Tipo di operazione richiesta
  
  int yes = 1;                                    //REUSE_ADDR in setsockopt richiede un parametro intero per optval                
  char command[COMMAND_LENGTH];                   //conterra' i comandi digitati

 
  struct timeval timeout;                         //Struttura per timeout sulla select
  timeout.tv_sec = 60;                            //Il client si disconnette in automatico dopo un minuto di inattività(stdin,udpSocket)
  timeout.tv_usec = 0;

  struct timeval *tm = NULL;
  fd_set readDescriptors; 
  int maxControlledDescriptors;
  FD_ZERO(&readDescriptors);
  int fd_pronti;                                  //descrittori pronti ritornati dalla select

  


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
    printf("Parameter's number is not valid.\nPlease use the following pattern: ./forza_client <host> <port>\n\n");
    exit(1);
  }

  if( (tcpSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      perror("A problem occured while creating Socket related to the Server!");           
      exit(1);
  }

  //Settaggio opzioni associate con la socket: Se indirizzo già in uso ritorno Errore
  if(setsockopt(tcpSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    perror("A problem occured while performing SETSOCKOPT() FUNCTION"); 
    close(tcpSocket);
    exit(1);
  }

  ipAddressServer = indirizzoServer;                    
  portServer = (unsigned short int)atoi(argv[2]); 

  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(portServer);
  inet_pton(AF_INET,ipAddressServer,&server.sin_addr.s_addr);
  
  ret = connect(tcpSocket, (struct sockaddr *)&server, sizeof(server));
  if(ret < 0) {
    perror("A problem occured while performing CONNECT() FUNCTION!");
    close(tcpSocket);
    exit(1);
  }

  printf("\nConnessione al server %s (porta: %d) effettuata con successo ",ipAddressServer,portServer);
  printf("\n");

  help(0); 
  
  login(tcpSocket, clientName, udpPort, tcpBuffer); //invia nome utente e porta udp al Server

  if(loginValidation() < 1) {
    perror("A problem occured while performing CONNECT() FUNCTION!");
    close(tcpSocket);
    exit(1);
  }

  //Creazione socket UDP

  if((udpSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("A problem occured while creating UDP Socket!"); 
    exit(1);
  }

  if(setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 ) {
    perror("A problem occured while performing SETSOCKOPT() FUNCTION"); 
    close(udpSocket);
    exit(1);
  }

  memset(&local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_port = htons(atoi(udpPort));
  local.sin_addr.s_addr = htonl(INADDR_ANY); 
  
  if(bind(udpSocket, (struct sockaddr*)&local,sizeof(local)) < 0) {
    perror("Error while executing bind()!\n\n"); 
    close(udpSocket);
    exit(1);
  }

  printf("\n> "); 
  fflush(stdout); 
  //------------------ AVVIO PARTITA --------------

  for(;;) {

    FD_SET(tcpSocket ,&readDescriptors);
    FD_SET(udpSocket, &readDescriptors);  
    FD_SET(fileno(stdin),&readDescriptors);         //fileno(FILE*f) ritorna file descriiptor dello stream
    maxControlledDescriptors = max(tcpSocket, udpSocket, fileno(stdin));     

    if( MATCH_STARTED == stateMatch) {                    //Quando la partita inizia inizializzo il timer
       timeout.tv_sec = 60;
       timeout.tv_usec = 10;
       tm = &timeout;//(struct timveval*)&timeout;
    } 
    else
      tm = NULL;

    if((fd_pronti = select(maxControlledDescriptors+1, &readDescriptors, NULL,NULL, tm)) <= 0) {

      if(fd_pronti < 0) {
        perror("Error while executing select()!\n\n"); 
        exit(1);
      }

      if(fd_pronti == 0) {                          //Avvenuto Timeout sulla select.Trascorso un minuto di inattività.
      
        if( stateMatch == MATCH_STARTED ) {            //Partita Avviata

          printf("INATTIVITA' PER UN MINUTO! AVVIO PROCEDURA DI DISCONNESIONE AUTOMATICA...\n");

          memset(udpBuffer,0,DIM_UDP_MSG);
          strcpy(udpBuffer,"TIMEOUT");

          if(sendUdpMessage(udpSocket,udpBuffer) <= 0) {
            printf("Error while trying to perfom sendUdpMessage() 'cause TIMEOUT occured.Termino connessione UDP\n");
            close(udpSocket);
            FD_CLR(udpSocket,&readDescriptors);
          }
          //Re-inizializzo le variabili stateMatch e turno. StateMatch = 0 (Parita non ancora avviata) turno = 0 (Non è ancora il proprio turno)
          stateMatch = 0;
          turno = 0;

          memset(tcpBuffer,0,DIM_TCP_MSG);   
          strcpy(tcpBuffer, "!disconnect"); 

          if(sendTcpMessage(tcpSocket, tcpBuffer) <= 0) {
            perror("Error while trying to perfom sendTcpMessage() 'cause TIMEOUT occured.Termino connessione TCP");
            close(tcpSocket);
            FD_CLR(tcpSocket,&readDescriptors);
            exit(1);
          }

          memset(map,0,42);
          memset(opponentName,0,CLIENT_NAME_LENGTH);
          printf("\t\tDISCONNESSIONE COMPLETATA CON SUCCESSO!\n\n");

          printf("> "); 
          fflush(stdout); 
        }
      }  
    }
    

    for(i = 0 ; i <= maxControlledDescriptors; i++) {

      if(FD_ISSET(i, &readDescriptors)) {

        if(i == tcpSocket) { 

          memset(tcpBuffer,0,DIM_TCP_MSG);

          if((nbytes = recvTcpMessage(i, tcpBuffer)) <= 0) {
            perror("Error while trying to execute recvTcpMessage()"); 
            close(tcpSocket);
            FD_CLR(tcpSocket, &readDescriptors);
            exit(1);
          }
      
          if((nbytes = evaluateOperation(tcpBuffer)) <= 0) {
            printf("Error while trying to execute evaluateOperation()."); 
            close(tcpSocket);
            FD_CLR(tcpSocket,&readDescriptors);
            exit(1);
          }

          if( nbytes == START ) {       //richiesta partita accettata. Avvio match
            memset(&opponentPeer, 0 , sizeof(opponentPeer));
            opponentPeer.sin_family = AF_INET;
            opponentPeer.sin_port = htons(atoi(opponentUdpPort));

            if(inet_pton(AF_INET, opponentIpAddress, &opponentPeer.sin_addr.s_addr) < 0) {
              perror("Error while trying to execute inet_pton function()");
              close(tcpSocket);
              FD_CLR(tcpSocket,&readDescriptors);
              exit(1);
            }
            
            ret = connect(udpSocket,(struct sockaddr *)&opponentPeer, sizeof(opponentPeer));        //connessione tra i peer

            if(ret < 0) {
              perror("Error while trying to execute connect function() beetween peers.");
              close(udpSocket);
              FD_CLR(udpSocket,&readDescriptors);
              close(tcpSocket);
              FD_CLR(tcpSocket,&readDescriptors);
              exit(1);
            }

            FD_SET(udpSocket, &readDescriptors);

            iniziaPartita();

            if(turno == 1){
              printf("# ");
              fflush(stdout);
            } 
            
          }
            break;
        }

          if(i == udpSocket) {                              //nuovo messaggio presente sulla porta udp

            nbytes = rcvUdpMessage(udpSocket,udpBuffer);

            if(nbytes <= 0) {
              perror("Error while trying to execute rcvUdpMessage function().");
              close(udpSocket);
              FD_CLR(udpSocket,&readDescriptors);
              break;
            }
    
            if(strcmp(udpBuffer,"HAI VINTO") == 0) {
              printf("\n\t\t*************** VITTORIA ***************\n");
              memset(map,0,42);
              memset(opponentName,0,CLIENT_NAME_LENGTH);
              stateMatch = 0;
              turno = 0;
              printf("> ");
              fflush(stdout);  
              break;            
            }

            if(evaluateUdpOperation(udpBuffer) == 1) { 
              
              nbytes = sendUdpMessage(udpSocket,udpBuffer);  

              if(nbytes <= 0) {
                 stateMatch = 0;
                 perror("Error while trying to execute sendUdpMessage function():");
                 close(udpSocket);
                 FD_CLR(udpSocket,&readDescriptors);
                 break;
              }

              if(strcmp(udpBuffer,"HAI VINTO") == 0) { //
              
                memset(tcpBuffer,0,DIM_TCP_MSG);
                strcpy(tcpBuffer,"!disconnect");
                nbytes = sendTcpMessage(tcpSocket,tcpBuffer);
                if(nbytes <= 0) {
                  perror("Error while trying to execute sendTcpMessage function():");
                  close(tcpSocket);
                  close(udpSocket);
                  exit(1);
                }
                memset(map,0,42);
                memset(opponentName,0,CLIENT_NAME_LENGTH);
              }
             
            } 
            break;
          }

          if(i == STDIN) {
                                                   
            memset(command,0,COMMAND_LENGTH);
            int res;
            fgets(command, sizeof(command), stdin);

            if((strlen(command) == COMMAND_LENGTH-1) && (command[strlen(command)] != '\n')) 
              while(getchar() != '\n');
          
            command[strlen(command)-1] = '\0';
    
            operation = whichCommand(command);

            switch(operation) {
              
              case -1:      
                              printf("Digita !help per la lista dei comandi.\n");
                              if( MATCH_STARTED == stateMatch )                                   //Partita avviata
                                printf("# ");
                              else
                              printf("> "); 
                              fflush(stdout); 
                              break;
      
              case 1: 
                              help(1);
                              break;

              case 2: 
                              if((nbytes = sendTcpMessage(tcpSocket,command)) <= 0){
                                  perror("Error while trying to execute sendTcpMessage().Can't send command required!\n");
                                  close(tcpSocket);
                                  FD_CLR(tcpSocket,&readDescriptors);
                                  exit(1);
                              }

                              break;

              case 3:
                              if(stateMatch == MATCH_STARTED) {
                                  printf("Hai deciso di abbandonare la partita\n");
                                  memset(tcpBuffer,0,sizeof(tcpBuffer));
                                  strcpy(tcpBuffer,"!disconnect");
                                  nbytes = sendTcpMessage(tcpSocket,tcpBuffer);
                                  if(nbytes <= 0) {
                                    perror("Error while trying to execute sendTcpMessage().Can't send command required!\n");
                                    close(tcpSocket);
                                    FD_CLR(tcpSocket,&readDescriptors);
                                    exit(1);
                                  }
                              }   
                                
                                  if((nbytes = sendTcpMessage(tcpSocket,command)) <= 0) {
                                    perror("Error while trying to execute sendTcpMessage().Can't send command required!\n");
                                    close(tcpSocket);
                                    FD_CLR(tcpSocket,&readDescriptors);
                                    exit(1);
                                  } 
                                recvTcpMessage(tcpSocket,tcpBuffer);
                                if(strcmp(tcpBuffer,"QUIT")!= 0)
                                printf("Terminazione forzata avvenuta!\n\n");
                                else
                                printf("Client disconnesso correttamente\n\n");
                                close(tcpSocket);
                                close(udpSocket);
                                FD_CLR(tcpSocket,&readDescriptors);
                                FD_CLR(udpSocket,&readDescriptors);
                                return 0;

              case 4:           
                                res = insertCoinIntoPersonalMap(command);
                                if( res == -1 )
                                  break; 
                                turno = 0;
                                memset(udpBuffer,0,sizeof(udpBuffer));
                                strcpy(udpBuffer,command);
                                nbytes = sendUdpMessage(udpSocket,udpBuffer);

                                if(nbytes <= 0) {
                                  stateMatch = 0;
                                  perror("Error while trying to execute sendUdpMessage().Can't send command required!\n");
                                  close(udpSocket);
                                  FD_CLR(tcpSocket,&readDescriptors);
                                }
                                break;
              case 5:
                                showMap();
                                break;

            }//fine switch()
        
      }//if(i==stdin)
    
    } //fine if(FD_ISsET())
     
  }//for(i;i<=fdmax..)
 
  
 }//fine for(;;) 
  
  return 0;
}
