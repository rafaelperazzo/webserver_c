/*****************************************************
 * Universidade de S�o Paulo - USP
 * Instituto de Matem�tica e Estat�stica - IME
 * Curso: P�s Gradua��o em Ci�ncia da Computa��o
 * Disciplina: Programa��o para redes de computadores
 * Professor: Daniel Batista (batista@ime.usp.br)
 * Aluno: Rafael Perazzo Barbosa Mota (perazzo@ime.usp.br)
 * NUSP: 5060192
 *
 * EP1: Servidor HTTP
 * Setembro de 2012
 * Funcionalidades:
 * M�todos: GET E POST
 * Erros: 404 (p�gina n�o encontrada), 403 (Forbidden) e 501 (M�todo n�o implementado)
 * Imagens nas p�ginas
 * Acessa automaticamente o index.html caso seja passada uma pasta na URL
 */


/**
*
* BIBLIOTECAS UTILIZADAS
*
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

/**
 * Parametros do servidor
 */
#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096
#define SERVERNAME  "Rafael Perazzo WebServer 1.0"	/* Nome do servidor */
#define PROTOCOLO   "HTTP/1.1"				        /* Protocolo HTTP (Vers�o) */
#define MAXBUF      (4096)				            /* Tamanho de buffer */
#define RFC1123     "%a, %d %b %Y %H:%M:%S GMT"	    /* Formato de hora para cabecalho*/
#define DIR_ROOT    "www"		                    /* Diretorio onde ficam as paginas */

/**
 * Estrutura utilizada para organizar a requisi��o de um cliente,
 * fazendo a divisao do path, "diretorio ou arquivo" e protocolo/versao
 */
typedef struct{
    char line[MAXBUF];
    char metodo[5];		         /* GET ou POST*/
    char recurso[1000];         /* Recurso requisitado pelo cliente. (Ex. index.html)*/
    char protocolo[20];         /* Versao do Protocolo http*/
    char post[800];             /* Dados POST caso o m�todo seja POST */

}Request;

/**
 * Estrutura utilizada para guardar os returns da funcao checkRequest
 */
typedef struct{
    char dir[MAXBUF];		    /* diretorio local do arquivo*/
    struct stat statBuffer;	/* Montar a estrutura para verifica��o de estado dos arquivos*/
    int n;			            /* Abrir o arquivo local como "inteiro" */
    int answerCOD;	            /* C�digo de resposta do protocolo. Ex: 200, 404,...*/
}CR_returns;

/**
 * Estrutura utilizada para organizar o cliente
 */
typedef struct{
    int socket;
    struct sockaddr_in destino;
}Host;

/**
 * Fun��o que retorna o tipo de content-type
 */
char *get_mime_type(char *name);

/**
 * Fun��o para montar um cabe�alho padr�o HTTP
 */
void httpHeader(int client_socket,char *mimeType, int tamanho, time_t ultAtu, int request_check, char *check_type);

/**
 * Fun��o que envia p�gina de erro para o cliente
 */
int returnErro(int client_socket, char *mensagem, int request_check);

/**
 * Fun��o para mensagens de erro no console
 */
void saidaErro(char msg[100]);

/**
 * Fun��o que verifica a Request solicitada pelo browser
 */
CR_returns checkRequest(Host client, Request client_request);

/**
 *Fun��o que ler/organiza a requisi��o do browser
 */
Request readRequest(Host client);

/**
 * Fun��o para enviar o arquivo
 */
void sendFile(int client_socket, CR_returns returns);

/**
 * Fun��o que envia resposta a requisi��o GET do browser
 */
void sendGETRequest(Host client, CR_returns returns);

/**
 * Fun��o que envia resposta a requisi��o POST do browser
 */
void sendPOSTRequest(Host client, CR_returns returns, Request client_request);

/**
* Fun��o que devolve a �ltima linha de uma requisi��o POST
*/
char *getLastLine(char *requisicao);

/**
* Fun��o que mostra os valores passados via POST
*/
void mostraPost(char *post);
/**
*INICIO DO PROGRAMA PRINCIPAL
*/
int main (int argc, char **argv) {
   /* Os sockets. Um que ser� o socket que vai escutar pelas conex�es
    * e o outro que vai ser o socket espec�fico de cada conex�o */
	int listenfd, connfd;
   /* Informa��es sobre o socket (endere�o e porta) ficam nesta struct */
	struct sockaddr_in servaddr;
   /* Retorno da fun��o fork para saber quem � o processo filho e quem
    * � o processo pai */
   pid_t childpid;
   /* Armazena linhas recebidas do cliente */
	char	recvline[MAXLINE + 1];
   /* Armazena o tamanho da string lida do cliente */
   ssize_t  n;

	if (argc != 2) {
      fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
      fprintf(stderr,"Vai rodar um servidor HTTP <Porta> TCP\n");
		exit(1);
	}

   /* Cria��o de um socket. Eh como se fosse um descritor de arquivo. Eh
    * possivel fazer operacoes como read, write e close. Neste
    * caso o socket criado eh um socket IPv4 (por causa do AF_INET),
    * que vai usar TCP (por causa do SOCK_STREAM), j� que o HTTP
    * funciona sobre TCP, e ser� usado para uma aplica��o convencional sobre
    * a Internet (por causa do n�mero 0) */
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket :(\n");
		exit(2);
	}

   /* Agora � necess�rio informar os endere�os associados a este
    * socket. � necess�rio informar o endere�o / interface e a porta,
    * pois mais adiante o socket ficar� esperando conex�es nesta porta
    * e neste(s) endere�os. Para isso � necess�rio preencher a struct
    * servaddr. � necess�rio colocar l� o tipo de socket (No nosso
    * caso AF_INET porque � IPv4), em qual endere�o / interface ser�o
    * esperadas conex�es (Neste caso em qualquer uma -- INADDR_ANY) e
    * qual a porta. Neste caso ser� a porta que foi passada como
    * argumento no shell (atoi(argv[1]))
    */
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(atoi(argv[1]));
	if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
		perror("bind :(\n");
		exit(3);
	}

   /* Como este c�digo � o c�digo de um servidor, o socket ser� um
    * socket passivo. Para isto � necess�rio chamar a fun��o listen
    * que define que este � um socket de servidor que ficar� esperando
    * por conex�es nos endere�os definidos na fun��o bind. */
	if (listen(listenfd, LISTENQ) == -1) {
		perror("listen :(\n");
		exit(4);
	}
   system("clear");
   printf("[Servidor no ar. Aguardando conexoes na porta %s]\n",argv[1]);
   printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");

   /* O servidor no final das contas � um loop infinito de espera por
    * conex�es e processamento de cada uma individualmente */
	for (;;) {
      /* O socket inicial que foi criado � o socket que vai aguardar
       * pela conex�o na porta especificada. Mas pode ser que existam
       * diversos clientes conectando no servidor. Por isso deve-se
       * utilizar a fun��o accept. Esta fun��o vai retirar uma conex�o
       * da fila de conex�es que foram aceitas no socket listenfd e
       * vai criar um socket espec�fico para esta conex�o. O descritor
       * deste novo socket � o retorno da fun��o accept. */
		if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
			perror("accept :(\n");
			exit(5);
		}

      /* Agora o servidor precisa tratar este cliente de forma
       * separada. Para isto � criado um processo filho usando a
       * fun��o fork. O processo vai ser uma c�pia deste. Depois da
       * fun��o fork, os dois processos (pai e filho) estar�o no mesmo
       * ponto do c�digo, mas cada um ter� um PID diferente. Assim �
       * poss�vel diferenciar o que cada processo ter� que fazer. O
       * filho tem que processar a requisi��o do cliente. O pai tem
       * que voltar no loop para continuar aceitando novas conex�es */
      /* Se o retorno da fun��o fork for zero, � porque est� no
       * processo filho. */
      if ( (childpid = fork()) == 0) {
         /**** PROCESSO FILHO ****/
         printf("[Uma conexao aberta]\n");
         /* J� que est� no processo filho, n�o precisa mais do socket
          * listenfd. S� o processo pai precisa deste socket. */
         close(listenfd);

         /* Agora pode ler do socket e escrever no socket. Isto tem
          * que ser feito em sincronia com o cliente. N�o faz sentido
          * ler sem ter o que ler. Ou seja, neste caso est� sendo
          * considerado que o cliente vai enviar algo para o servidor.
          * O servidor vai processar o que tiver sido enviado e vai
          * enviar uma resposta para o cliente (Que precisar� estar
          * esperando por esta resposta)
          */

         /* ========================================================= */
         /* ========================================================= */
         /*                         EP1 IN�CIO                        */
         /* ========================================================= */
         /* ========================================================= */
         /* TODO: � esta parte do c�digo que ter� que ser modificada
          * para que este servidor consiga interpretar comandos HTTP */

         /* Definindo o cliente */
         Host cliente;
         cliente.destino = servaddr;
         cliente.socket = connfd;

         /* Ler a requisi��o do cliente*/
         Request client_request = readRequest(cliente);

         /* Mostrar na tela as requisi��es dos clientes */
         printf("*********************************************************************");
         printf("\n [Metodo requisitado]:   %s \n", client_request.metodo);
         printf("\n [Recurso solicitado]:   %s \n", client_request.recurso);
         printf("\n [Protocolo]:            %s \n", client_request.protocolo);

         /* checar a Request do client */
         CR_returns returns =  checkRequest(cliente, client_request);

         /* enviar resposta da requisi��o para o cliente */
         if (strcmp(client_request.metodo,"GET")==0) {
            sendGETRequest(cliente, returns);
         }
         else if (strcmp(client_request.metodo,"POST")==0) {
            sendPOSTRequest(cliente, returns,client_request);

         }

         /* ========================================================= */
         /* ========================================================= */
         /*                         EP1 FIM                           */
         /* ========================================================= */
         /* ========================================================= */

         /* Ap�s ter feito toda a troca de informa��o com o cliente,
          * pode finalizar o processo filho */
         printf("[Uma conexao fechada]\n");
         exit(0);
      }
      /**** PROCESSO PAI ****/
      /* Se for o pai, a �nica coisa a ser feita � fechar o socket
       * connfd (ele � o socket do cliente espec�fico que ser� tratado
       * pelo processo filho) */
		close(connfd);
	}
	exit(0);
}

/**
 * Fun��o que retorna o tipo de content-type
 */
char *get_mime_type(char *name){
	char *ext = strrchr(name, '.');
	if (!ext) return NULL;
	if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(ext, ".gif") == 0) return "image/gif";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".au") == 0) return "audio/basic";
	if (strcmp(ext, ".wav") == 0) return "audio/wav";
	if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
	if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
	if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
	return NULL;
}

/**
 * Fun��o para montar um cabe�alho de resposta padr�o HTTP
 * igual ao retornado pelo Apache.
 */
void httpHeader(int client_socket,char *mimeType, int tamanho, time_t ultAtu, int request_check, char *check_type) {

    /* Preparando a data e hora */
	time_t agora;
	char bufHora[128];
	char buffer[100];

    /* Preparando o protocolo */
	sprintf(buffer, "%s %d %s\r\n", PROTOCOLO, request_check,check_type);
	write(client_socket, buffer, strlen(buffer));

    /* Preparando o nome do servidor */
	sprintf(buffer, "Server: %s\r\n", SERVERNAME);
	write(client_socket, buffer, strlen(buffer));

    /* Preparando a data/hota */
	agora = time(NULL);
	strftime(bufHora, sizeof(bufHora), RFC1123, gmtime(&agora));
    sprintf(buffer, "Date: %s\r\n", bufHora);
	write(client_socket, buffer, strlen(buffer));

    /* Preparando o tipo de conte�do */
	sprintf(buffer, "Content-Type: %s\r\n", mimeType);
	write(client_socket, buffer, strlen(buffer));

    /* Preparando o tamanho */
	if (tamanho >= 0) {
		sprintf(buffer, "Content-Length: %d\r\n", tamanho);
		write(client_socket, buffer, strlen(buffer));
	}

    /* Preparando a data/hora da �ltima modifica��o para efeitos de cache */
	if (ultAtu != -1){
		strftime(bufHora, sizeof(bufHora), RFC1123, gmtime(&ultAtu));
		sprintf(buffer, "Last-Modified: %s\r\n", bufHora);
		write(client_socket, buffer, strlen(buffer));
   	}

    /* Enviando linha em branco para finalizar o cabe�alho */
	write(client_socket, "\r\n", 2);
}

/**
 * Fun��o que envia p�gina de erro para o cliente
 */
int returnErro(int client_socket, char *mensagem, int request_check) {
	char buffer[100];

    /* Preparando cabe�alho de resposta */
    if (request_check==404) {
        httpHeader(client_socket,"text/html",-1,-1, request_check, "Pagina nao encontrada");
    }
    else if (request_check==501) {
        httpHeader(client_socket,"text/html",-1,-1, request_check, "Metodo nao implementado");
    }
    else if (request_check==403) {
        httpHeader(client_socket,"text/html",-1,-1, request_check, "Forbidden");
    }

    /* Montando a p�gina de erro */
	sprintf(buffer, "<html>\n<head>\n<title>%s : Erro %d</title>\n</head>\n\n", SERVERNAME,request_check);
	write(client_socket, buffer, strlen(buffer));

	sprintf(buffer, "<body>\n<h1>%s : Erro %d</h1>\n", SERVERNAME,request_check);
	write(client_socket, buffer, strlen(buffer));

	sprintf(buffer, "<p>%s</p>\n</body>\n</html>\n",mensagem);
	write(client_socket, buffer, strlen(buffer));

	return 0;
}

/**
 * Fun��o para mensagens de erro no console
 */
void saidaErro(char msg[100]){
	fprintf(stderr, "%s: %s\r\n", SERVERNAME, msg);
	exit(EXIT_FAILURE);
}

/**
 * Fun��o que verifica a Request solicitada pelo browser
 */
CR_returns checkRequest(Host client, Request client_request) {

	CR_returns returns;

	/*Guardar informa��es sobre o caminho do recurso*/
	char pathbuf[MAXBUF];

	/*Para auxliar na contagem de tamanho do "caminho"*/
	int len;

    /* Copiar para a vari�vel dir o caminho do DIR_ROOT*/
	strcpy(returns.dir,DIR_ROOT);

	/*Se existir o recurso, concatena com a variavel dir, se nao complementa com a pagina inicial /index.html*/
	if(client_request.recurso)
		strcat(returns.dir,client_request.recurso);
	else strcat(returns.dir,"/index.html");

	printf("\n [Pasta]                      %s\n", returns.dir);
	printf("\n ************************************************************************\n\n");

	/* Se o tipo de m�todo nao existir returns com codigo 501 */
	if((strcmp(client_request.metodo, "GET") != 0)&&(strcmp(client_request.metodo, "POST") != 0))
		{returns.answerCOD = 501; return returns;}

	/* Se o arquivo nao existe returns com check 404 */
	if(stat(returns.dir, &returns.statBuffer) == -1)
		{returns.answerCOD = 404; return returns;}

	/* Se o arquivo for encontrado e for um diretorio */
	if(S_ISDIR(returns.statBuffer.st_mode)) { //Inicio if 1
		len = strlen(returns.dir);

		/* Se n�o colocar a barra no final da url, no caso de ser um diret�rio*/
		if (len == 0 || returns.dir[len - 1] != '/'){ //Inicio if 2

			/* Adicionar a "/index.html" barra no final da URL(pelo menos dentro do servidor)*/
			snprintf(pathbuf, sizeof(pathbuf), "%s/", returns.dir);
			strcat(returns.dir,"/index.html");

			/* Se existir o index.html dentro do diretorio, ent�o returns com check == 200, se nao existir check == 404*/
			if(stat(returns.dir, &returns.statBuffer) == -1)
				{returns.answerCOD = 404; return returns;}
			else
				{returns.answerCOD = 200; return returns;}
		}
		else{
			/* Se n�o tiver colocar "/" depois do nome do diret�rio ent�o colocar� apenas index.html*/
			snprintf(pathbuf, sizeof(pathbuf), "%sindex.html", returns.dir);
			strcat(returns.dir,"index.html");

			/* Se existir o index.html dentro do diretorio, ent�o returns com answerCOD == 200, se nao existir answerCOD == 404*/
			if(stat(returns.dir, &returns.statBuffer) >= 0)
				{returns.answerCOD = 200; return returns;}
			else
				{returns.answerCOD = 404; return returns;}
		} //Fim if 2
	}
	/* Se for um arquivo que nao seja um diretorio returns com answerCOD == 200, com o endere�o da url completo (statBuffer) */
	else
		{returns.answerCOD = 200; return returns;} //Fim if 1
}

/**
 *Fun��o que recebe e organiza a requisi��o do cliente
 */
Request readRequest(Host client) {

    ssize_t  n;
	Request client_request;
	char line[MAXBUF];
	char *metodo = malloc(5);
	char *recurso = malloc(1000);
	char *protocolo = malloc(20);
	char *post = malloc(800);

    /*Lendo requisi��o do cliente*/
    n=read(client.socket, line, MAXBUF);

	/* Colocar os dados do socket(requisicao completa) na vari�vel "line" */
	strcpy(client_request.line, line);

	/* Manipulando a variavel line para extrair dados como o m�todo, recurso, protocolo e post*/
	metodo = strtok(line, " ");
	recurso = strtok(NULL, " ");
	protocolo = strtok(NULL, "\r");
	strcpy(client_request.metodo, metodo);
	strcpy(client_request.recurso,recurso);
	strcpy(client_request.protocolo, protocolo);
    if (strcmp(client_request.metodo,"POST")==0) {
        strcpy(client_request.post, getLastLine(client_request.line));
    }
	return client_request;
}

/**
 * Fun��o para enviar o arquivo
 */
void sendFile(int client_socket, CR_returns returns){
	char dados;
	int i;

    /* Abrindo arquivo */
	FILE *arq = fopen(returns.dir, "r");

	/* Ver se o sistema tem acesso ao arquivo */
	if (!arq) {
		returns.answerCOD = 403;
		httpHeader(client_socket,"text/html",-1,-1, returns.answerCOD, "Forbidden");
		returnErro(client_socket,"Acesso Negado", returns.answerCOD);
	}
	else {
		int tamanho = S_ISREG(returns.statBuffer.st_mode) ? returns.statBuffer.st_size : -1;
        /* Preparando cabe�alho de resposta */
		httpHeader(client_socket,get_mime_type(returns.dir),tamanho ,returns.statBuffer.st_mtime, returns.answerCOD, "OK");

        /*Enviando arquivo */
		while ((i = read(returns.n, &dados,1)))
			write(client_socket, &dados, 1);

	}
}

/**
 * Fun��o que envia resposta a requisi��o GET do browser
 */
void sendGETRequest(Host client, CR_returns returns){
	if (returns.answerCOD == 501) // M�todo n�o suportado pelo servidor
	        returnErro(client.socket,"<h2>Metodo nao suportado</h2>", returns.answerCOD);

	if (returns.answerCOD == 404) // Arquivo n�o encontrado
	        returnErro(client.socket,"<h2>Pagina nao encontrada</h2>", returns.answerCOD);

    if (returns.answerCOD == 403) // Arquivo sem permiss�o de leitura/execu��o
	        returnErro(client.socket,"<h2>Forbidden</h2>", returns.answerCOD);

	if (returns.answerCOD == 200){ //Arquivo OK
	        returns.n = open(returns.dir, O_RDONLY);
	        sendFile(client.socket,returns);
	}
}

/**
 * Fun��o que envia resposta a requisi��o POST do browser
 */
void sendPOSTRequest(Host client, CR_returns returns, Request client_request) {

	char buffer[100];
    if (returns.answerCOD == 501) // M�todo n�o suportado pelo servidor
	        returnErro(client.socket,"<h2>Metodo nao suportado</h2>", returns.answerCOD);

	if (returns.answerCOD == 404) // Arquivo n�o encontrado
	        returnErro(client.socket,"<h2>Pagina nao encontrada</h2>", returns.answerCOD);

    if (returns.answerCOD == 403) // Arquivo sem permiss�o de leitura/execu��o
	        returnErro(client.socket,"<h2>Forbidden</h2>", returns.answerCOD);

	if (returns.answerCOD == 200){ //Arquivo OK
	        /*Preparando os objetos posts*/
	        int cont = contaPosts(client_request.post);

	        /* Preparando cabe�alho de resposta */
            httpHeader(client.socket,"text/html",-1,-1, returns.answerCOD, "OK");

            /* Montando a p�gina resultado de um POST */
            sprintf(buffer, "<html>\n<head>\n<title>Resultado do POST</title>\n</head>\n\n");
            write(client.socket, buffer, strlen(buffer));

            /* Imprimindo listagem de objetos */
            sprintf(buffer, "<body>\n<h2>Vari�veis POST</h2><BR>\n");
            write(client.socket, buffer, strlen(buffer));
            char *tockens;
            /*Acessando os dados recebidos via POST*/
            tockens = strtok (client_request.post,"=&");
            sprintf(buffer,"<h4>");
            write(client.socket, buffer, strlen(buffer));
            int i = 0;
            /*Imprimindo na tela nome da variaval = valor da variavel*/
            while (tockens!=NULL) {
                if ((i%2)==0) {
                    sprintf(buffer, "Vari�vel:  %s", tockens);
                    write(client.socket,buffer,strlen(buffer));
                }
                else {
                    sprintf(buffer, " =<i> %s<BR>\n</i>", tockens);
                    write(client.socket,buffer,strlen(buffer));
                }
                tockens = strtok(NULL,"=&");
                i++;
            }
            sprintf(buffer,"</h4>");
            write(client.socket, buffer, strlen(buffer));

            /* Fechando o HTML */
            sprintf(buffer, "\n</body>\n</html>\n");
            write(client.socket, buffer, strlen(buffer));
	}



}

/**
 * Fun��o que retorna a �ltima linha de uma requisi��o POST
 */
char *getLastLine(char *requisicao) {

    int contador = 0;
    int i;
    int pos1=0,pos2=0;
    int tamanho = 0;
    tamanho = strlen(requisicao);
    for (i=0;i<tamanho;i++) {
        if (requisicao[i]=='\n') {
            contador++;
        }
    }
    int *posicoes = (int*) malloc(contador);

    printf("\n");
    int j = 0;
    for (i=0;i<tamanho;i++) {
        if (requisicao[i]=='\n') {
            posicoes[j] = i;
            j++;
        }
    }
    printf("\n");
    pos1 = posicoes[contador-3];
    pos2 = posicoes[contador-1];

    char *line = (char*) malloc(pos2-pos1);
    strncpy(line,requisicao+pos2,tamanho-pos2);

    return line;
}

/**
 * Fun��o que conta quantos objetos posts foram enviados ao servidor
 */
int contaPosts(char *post) {

    int i;
    int contador = 0; /*Conta quantas vezes o simbolo & aparece */
    for (i=0;i<strlen(post);i++) {
        if (post[i]=='&') {
            contador++;
        }
    }
    return (contador+1);

}
