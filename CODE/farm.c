#include <boundedqueue.h>
#include <myUtil.h>

#define MAX_STRING 100
#define UNIX_PATH_MAX 108
#define EOS (void*)0x1
#define SOCKNAME "./farm.sck"

volatile sig_atomic_t exitval=0;
static pthread_mutex_t mtx=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t rd=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t wr=PTHREAD_MUTEX_INITIALIZER;

void handlerINT(int s){
  //incremento variabile per uscire dalla push
  //write(1,"ciao\n",6);
  exitval=1;
}

typedef struct threadArg{
  BQueue_t *q;
  int thid;
} threadArgs_t;

static int aggiorna (int fd_num, fd_set *set) {
  while (!FD_ISSET(fd_num,set))
    fd_num--;
  return fd_num;
}

//processo thread 
void *Work(void* arg){
  BQueue_t *q=((threadArgs_t*)arg)->q; //coda
  //int myid=((threadArgs_t*)arg)->thid;

  while(1){
    char *buffer=pop(q);
    if (buffer==EOS) break;
    if(strcmp(buffer,"quit")==0){ //invio segnale chiusura collector

      //creo collegamento server
      struct sockaddr_un server;
      strncpy(server.sun_path, SOCKNAME, UNIX_PATH_MAX);
      server.sun_family = AF_UNIX;

      int fd_s=socket(AF_UNIX,SOCK_STREAM, 0); //creo socket
      //provo a connettermi col collector        
      while (connect(fd_s,(struct sockaddr*)&server, sizeof(server))==-1){
        if (errno==ENOENT) sleep(1); // sock non esiste 
        else 
          exit(EXIT_FAILURE); 
      }
      LOCK(&wr); //scrivo al collector dal thread
      write(fd_s,"quit",strlen("quit"));
      UNLOCK(&wr);
      close(fd_s);
      return NULL;

    }else{
      FILE *file;
      if((file=fopen(buffer,"rb"))==NULL){
        printf("Can't open file\n");
        if(unlink("./farm.sck")!=0) perror("Error unlink");
        exit(EXIT_FAILURE);
      }else{
      
        struct stat statbuf;
        if (stat(buffer, &statbuf) == -1) break;
        int size = (int)statbuf.st_size;
        long int elem=size/8;

        LOCK(&mtx);
        long int res[elem];
        int count=0;
        long int sum=0, i=0;
        while (count<elem){
          fread(&res[count++], sizeof(long int), 1, file); 
          sum=sum+(i*res[i]);
          i++;
        }
        UNLOCK(&mtx);
      
        char str[MAX_STRING];
        memset(str,0,sizeof(str)); //all 0
        sprintf(str, "%ld", sum); //convert long int to char
        strcat(str, " ");
        strcat(str, buffer);

        //creo collegamento server
        struct sockaddr_un server;
        strncpy(server.sun_path, SOCKNAME, UNIX_PATH_MAX);
        server.sun_family = AF_UNIX;

        int fd_s=socket(AF_UNIX,SOCK_STREAM, 0); //creo socket
        //provo a connettermi col collector        
        while (connect(fd_s,(struct sockaddr*)&server, sizeof(server))==-1){
          if (errno==ENOENT) sleep(1); // sock non esiste 
          else 
            exit(EXIT_FAILURE); 
        }
        LOCK(&wr); //scrivo al collector dal thread
        write(fd_s, str,strlen(str));
        UNLOCK(&wr);
        close(fd_s);
      }
      fclose(file);
    }
  }
  return NULL;
}


//processo main - collector
int main(int argc, char* argv[]){

  int N=4,Q=8,T=0,start=7,file_to_read=0, i; //flag per uscire while
  long n;
  //check parametri
  if (argc<2){
    printf("Pochi parametri!\nInserisci [opzionali: N Thread, Q coda, T time] lista file!\n");
    return -1;
  }else if(argc==2){
    start=1;
  }else if(argc>2){
    for(int i=1; i<=5; i=i+2){
      if (strcmp(argv[i],"-n")==0){
        if((isNumber(argv[i+1],&n))!=0){
          printf("Error parameter not number!\n");
          exit(EXIT_FAILURE);
        }else{
          N=atoi(argv[i+1]); //N workers
        }
        continue;
      }
      if (strcmp(argv[i],"-q")==0){
        if((isNumber(argv[i+1],&n))!=0){
          printf("Error parameter not number!\n");
          exit(EXIT_FAILURE);
        }else{
          Q=atoi(argv[i+1]); //Q queue
        }
        continue;
      }
      if (strcmp(argv[i],"-t")==0){
        if((isNumber(argv[i+1],&n))!=0){
          printf("Error parameter not number!\n");
          exit(EXIT_FAILURE);
        }else{
          T=atoi(argv[i+1]); //T time
        }
        continue;
      }
      start=i;
      break;
    }
  }
  file_to_read=argc-start;

  //strutture per gestire segnali
  struct sigaction s;
  memset(&s,0,sizeof(s));
  //maschero segnali
  sigset_t set;
  SYSCALL_EXIT("sigfillset",i,sigfillset(&set)); //creo ua mschera per tutti i segnali
  SYSCALL_EXIT("pthreadmask",i,pthread_sigmask(SIG_SETMASK,&set,NULL));
  //prendo valore default struct
  SYSCALL_EXIT("sigaction",i,sigaction(SIGPIPE,NULL,&s));
  //inizio a settare i campi degli handler che voglio cambiare
  s.sa_handler=SIG_IGN; //ignoro segnale sigpipe
  SYSCALL_EXIT("sigactionPIPE",i,sigaction(SIGPIPE,&s,NULL));
  //SYSCALL_EXIT("sigaction", i,sigaction(SIGINT,NULL,&s));
  s.sa_handler=handlerINT;
  s.sa_flags=SA_RESTART;
  SYSCALL_EXIT("sigactionINT", i,sigaction(SIGINT,&s,NULL));
  SYSCALL_EXIT("sigactionTERM",i,sigaction(SIGTERM,&s,NULL));
  SYSCALL_EXIT("sigactionQUIT",i,sigaction(SIGQUIT,&s,NULL));
  SYSCALL_EXIT("sigactionHUP",i,sigaction(SIGHUP,&s,NULL));
  sigdelset(&set,SIGINT);
  sigdelset(&set,SIGTERM);
  sigdelset(&set,SIGQUIT);
  sigdelset(&set,SIGHUP);
  sigdelset(&set,SIGPIPE);
  SYSCALL_EXIT("pthreadmask",i,pthread_sigmask(SIG_SETMASK,&set,NULL));

  pthread_t     *workers;
  threadArgs_t *thrArg;
  thrArg=malloc(N*sizeof(threadArgs_t));
  workers=malloc(N*sizeof(pthread_t));
  if(!thrArg || !workers){
    perror("Error malloc");
    exit(EXIT_FAILURE);
  }
  BQueue_t *q=initBQueue(Q);
  assert(q);
  size_t filesize;
  pid_t pid=fork();

  //sono processo padre pid!=0 clients - thread 
  if(pid!=0){
    //inizio a creare i vari thread con proprie struct
    for (int i=0; i<N; i++){
      thrArg->thid=i+1; //myid
      thrArg->q=q; //coda elementi
      CREATE_T(&workers[i],Work,thrArg); //creo thread
    }
    //dopo pusho i file nella coda
    int cont=0;
    for (int i=start; i<argc; i++){
      if(exitval==1) break; 
      if(isRegular(argv[i],&filesize)!=1){
        printf("Error file: %s not regular!\n",argv[i]);
        if(unlink("./farm.sck")!=0) perror("Error unlink");
        exit(EXIT_FAILURE);
      }
      if(push(q,argv[i])==-1){
        perror("Error push");
        exit(EXIT_FAILURE);
      }
      cont++;
      usleep(T*1000);
    }
    //mando segnale terminazione dei workers
    if(cont<argc-start){ //arrivato segnale -> push N-1 EOS e 1 QUIT
      for (int i=0; i<N-1; i++){ 
        if(push(q,EOS)==-1){
          perror("Error push");
          exit(EXIT_FAILURE);
        }    
      }
      if(push(q,"quit")==-1){
        perror("Error push");
        exit(EXIT_FAILURE);
      }      
    }else{ //no segnale -> push N volte EOS
      for (int i=0; i<N; i++){ 
        if(push(q,EOS)==-1){
          perror("Error push");
          exit(EXIT_FAILURE);
        }    
      }
    }
    //aspetto terminazione workers
    for (int i=0; i<N; i++){
      JOIN_T(workers[i]);
    }
    //printf("Aspetto thread\n");
    //dealloco
    waitpid(0,NULL,0); //aspetto terminazione figlio
    deleteBQueue(q,NULL); //delete coda
    free(workers); //free threads
    free(thrArg);
    unlink("./farm.sck"); //unlink farm.sck
    exit(EXIT_SUCCESS);
  }

  if(pid==0){ //processo figlio - run_collector
    //figlio con fork eredita gestione dei segnali del padre
    fd_set set, rdset;
    int fd_client,fd_max=0,nr, fd_i, readf=0;

    //creo fd socket del collector, struct server e collego col server 
    struct sockaddr_un server;
    strncpy(server.sun_path,SOCKNAME,UNIX_PATH_MAX);
    server.sun_family=AF_UNIX;
    int fd_socket=socket(AF_UNIX,SOCK_STREAM, 0);

    if(bind(fd_socket, (struct sockaddr*)&server, sizeof(server))==-1){ //collego il fd socket ad un server
      perror("Error bind");
      unlink("./farm.sck");
      exit(EXIT_FAILURE);
    }
    if(listen(fd_socket,SOMAXCONN)==-1){ //ascolto un max di SOMAXCONN client
      perror("Error listen");
      unlink("./farm.sck");
      exit(EXIT_FAILURE);
    }
    //setto maschere per socket
    if(fd_socket>fd_max) fd_max=fd_socket;
    FD_ZERO(&set);
    FD_SET(fd_socket,&set);

		while(1){
      
      int l;
      rdset=set;
      //vado in attesa se non e disponibile un fd
      while((l=select(fd_max+1, &rdset, NULL,NULL,NULL))==-1 && errno==EINTR){
      }
      if(l<0){
        perror("error select");
        unlink("./farm.sck");
        exit(EXIT_FAILURE);
      }else{
        //select andata buon fine -> controllo quale file descriptor e pronto
        for(fd_i=0; fd_i<=fd_max; fd_i++){
          if(FD_ISSET(fd_i,&rdset)){ //sock pronto
            if(fd_i==fd_socket){
              //accetto connessione con client e metto il fd nel set
              fd_client=accept(fd_socket, NULL, 0);
              FD_SET(fd_client, &set);
              if(fd_client>fd_max)  fd_max=fd_client;
            }else{ //sock I/O pronto
              char buff[MAX_STRING];
              memset(buff,0,MAX_STRING);
              LOCK(&rd);
              nr=read(fd_i,buff,MAX_STRING);
              if(nr==-1){
                break;
              }else if(nr==0){
                FD_CLR(fd_i,&set);
                fd_max=aggiorna(fd_max,&set);
                if(readf>=file_to_read || strcmp(buff,"quit")==0){
                  UNLOCK(&rd);
                  close(fd_i);
                  close(fd_socket);
                  //printf("uscita collector\n");
                  exit(EXIT_SUCCESS);
                }
                close(fd_i);
              }else{
                buff[nr]='\0';
                if(readf>=file_to_read || strcmp(buff,"quit")==0){
                  UNLOCK(&rd);
                  close(fd_i);
                  close(fd_socket);
                  //printf("uscita collector\n");
                  exit(EXIT_SUCCESS);
                }
                printf("%s\n", buff);
                readf++;
              }
              UNLOCK(&rd);
            }
          }
        }
      }
    }
    exit(EXIT_SUCCESS);
  }
  return 0;
}
