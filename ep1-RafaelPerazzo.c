/*****************************************************
 * Universidade de São Paulo - USP
 * Instituto de Matemática e Estatística - IME
 * Curso: Pós Graduação em Ciência da Computação
 * Disciplina: Programação para redes de computadores
 * Professor: Daniel Batista (batista@ime.usp.br)
 * Aluno: Rafael Perazzo Barbosa Mota (perazzo@ime.usp.br)
 * NUSP: 5060192
 *
 * EP1: Servidor HTTP
 * Setembro de 2012
 * Funcionalidades:
 * Métodos: GET E POST
 * Erros: 404 (página não encontrada), 403 (Forbidden) e 501 (Método não implementado)
 * Imagens nas páginas
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
#define PROTOCOLO   "HTTP/1.1"				        /* Protocolo HTTP (Versão) */
#define MAXBUF      (4096)				            /* Tamanho de buffer */
#define RFC1123     "%a, %d %b %Y %H:%M:%S GMT"	    /* Formato de hora para cabecalho*/
#define DIR_ROOT    "www"		                    /* Diretorio onde ficam as paginas */

/**
 * Estrutura utilizada para organizar a requisição de um cliente,
 * fazendo a divisao do path, "diretorio ou arquivo" e protocolo/versao
 */
typedef struct{
    char line[MAXBUF];
    char metodo[5];		         /* GET ou POST*/
    char recurso[1000];         /* Recurso requisitado pelo cliente. (Ex. index.html)*/
    char protocolo[20];         /* Versao do Protocolo http*/
    char post[800];             /* Dados POST caso o método seja POST */

}Request;

/**
 * Estrutura utilizada para guardar os returns da funcao checkRequest
 */
typedef struct{
    char dir[MAXBUF];		    /* diretorio local do arquivo*/
    struct stat statBuffer;	/* Montar a estrutura para verificação de estado dos arquivos*/
    int n;			            /* Abrir o arquivo local como "inteiro" */
    int answerCOD;	            /* Código de resposta do protocolo. Ex: 200, 404,...*/
}CR_returns;

/**
 * Estrutura utilizada para organizar o cliente
 */
typedef struct{
    int socket;
    struct sockaddr_in destino;
}Host;

/**
 * Função que retorna o tipo de content-type
 */
char *get_mime_type(char *name);

/**
 * Função para montar um cabeçalho padrão HTTP
 */
void httpHeader(int client_socket,char *mimeType, int tamanho, time_t ultAtu, int request_check, char *check_type);

/**
 * Função que envia página de erro para o cliente
 */
int returnErro(int client_socket, char *mensagem, int request_check);

/**
 * Função para mensagens de erro no console
 */
void saidaErro(char msg[100]);

/**
 * Função que verifica a Request solicitada pelo browser
 */
CR_returns checkRequest(Host client, Request client_request);

/**
 *Função que ler/organiza a requisição do browser
 */
Request readRequest(Host client);

/**
 * Função para enviar o arquivo
 */
void sendFile(int client_socket, CR_returns returns);

/**
 * Função que envia resposta a requisição GET do browser
 */
void sendGETRequest(Host client, CR_returns returns);

/**
 * Função que envia resposta a requisição POST do browser
 */
void sendPOSTRequest(Host client, CR_returns returns, Request client_request);

/**
* Função que devolve a última linha de uma requisição POST
*/
char *getLastLine(char *requisicao);

/**
* Função que mostra os valores passados via POST
*/
void mostraPost(char *post);
/**
*INICIO DO PROGRAMA PRINCIPAL
*/
int main (int argc, char **argv) {
   /* Os sockets. Um que será o socket que vai escutar pelas conexões
    * e o outro que vai ser o socket específico de cada conexão */
	int listenfd, connfd;
   /* Informações sobre o socket (endereço e porta) ficam nesta struct */
	struct sockaddr_in servaddr;
   /* Retorno da função fork para saber quem é o processo filho e quem
    * é o processo pai */
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

   /* Criação de um socket. Eh como se fosse um descritor de arquivo. Eh
    * possivel fazer operacoes como read, write e close. Neste
    * caso o socket criado eh um socket IPv4 (por causa do AF_INET),
    * que vai usar TCP (por causa do SOCK_STREAM), já que o HTTP
    * funciona sobre TCP, e será usado para uma aplicação convencional sobre
    * a Internet (por causa do número 0) */
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket :(\n");
		exit(2);
	}

   /* Agora é necessário informar os endereços associados a este
    * socket. É necessário informar o endereço / interface e a porta,
    * pois mais adiante o socket ficará esperando conexões nesta porta
    * e neste(s) endereços. Para isso é necessário preencher a struct
    * servaddr. É necessário colocar lá o tipo de socket (No nosso
    * caso AF_INET porque é IPv4), em qual endereço / interface serão
    * esperadas conexões (Neste caso em qualquer uma -- INADDR_ANY) e
    * qual a porta. Neste caso será a porta que foi passada como
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

   /* Como este código é o código de um servidor, o socket será um
    * socket passivo. Para isto é necessário chamar a função listen
    * que define que este é um socket de servidor que ficará esperando
    * por conexões nos endereços definidos na função bind. */
	if (listen(listenfd, LISTENQ) == -1) {
		perror("listen :(\n");
		exit(4);
	}
   system("clear");
   printf("[Servidor no ar. Aguardando conexoes na porta %s]\n",argv[1]);
   printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");

   /* O servidor no final das contas é um loop infinito de espera por
    * conexões e processamento de cada uma individualmente */
	for (;;) {
      /* O socket inicial que foi criado é o socket que vai aguardar
       * pela conexão na porta especificada. Mas pode ser que existam
       * diversos clientes conectando no servidor. Por isso deve-se
       * utilizar a função accept. Esta função vai retirar uma conexão
       * da fila de conexões que foram aceitas no socket listenfd e
       * vai criar um socket específico para esta conexão. O descritor
       * deste novo socket é o retorno da função accept. */
		if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
			perror("accept :(\n");
			exit(5);
		}

      /* Agora o servidor precisa tratar este cliente de forma
       * separada. Para isto é criado um processo filho usando a
       * função fork. O processo vai ser uma cópia deste. Depois da
       * função fork, os dois processos (pai e filho) estarão no mesmo
       * ponto do código, mas cada um terá um PID diferente. Assim é
       * possível diferenciar o que cada processo terá que fazer. O
       * filho tem que processar a requisição do cliente. O pai tem
       * que voltar no loop para continuar aceitando novas conexões */
      /* Se o retorno da função fork for zero, é porque está no
       * processo filho. */
      if ( (childpid = fork()) == 0) {
         /**** PROCESSO FILHO ****/
         printf("[Uma conexao aberta]\n");
         /* Já que está no processo filho, não precisa mais do socket
          * listenfd. Só o processo pai precisa deste socket. */
         close(listenfd);

         /* Agora pode ler do socket e escrever no socket. Isto tem
          * que ser feito em sincronia com o cliente. Não faz sentido
          * ler sem ter o que ler. Ou seja, neste caso está sendo
          * considerado que o cliente vai enviar algo para o servidor.
          * O servidor vai processar o que tiver sido enviado e vai
          * enviar uma resposta para o cliente (Que precisará estar
          * esperando por esta resposta)
          */

         /* ========================================================= */
         /* ========================================================= */
         /*                         EP1 INÍCIO                        */
         /* ========================================================= */
         /* ========================================================= */
         /* TODO: É esta parte do código que terá que ser modificada
          * para que este servidor consiga interpretar comandos HTTP */

         /* Definindo o cliente */
         Host cliente;
         cliente.destino = servaddr;
         cliente.socket = connfd;

         /* Ler a requisição do cliente*/
         Request client_request = readRequest(cliente);

         /* Mostrar na tela as requisições dos clientes */
         printf("*********************************************************************");
         printf("\n [Metodo requisitado]:   %s \n", client_request.metodo);
         printf("\n [Recurso solicitado]:   %s \n", client_request.recurso);
         printf("\n [Protocolo]:            %s \n", client_request.protocolo);

         /* checar a Request do client */
         CR_returns returns =  checkRequest(cliente, client_request);

         /* enviar resposta da requisição para o cliente */
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

         /* Após ter feito toda a troca de informação com o cliente,
          * pode finalizar o processo filho */
         printf("[Uma conexao fechada]\n");
         exit(0);
      }
      /**** PROCESSO PAI ****/
      /* Se for o pai, a única coisa a ser feita é fechar o socket
       * connfd (ele é o socket do cliente específico que será tratado
       * pelo processo filho) */
		close(connfd);
	}
	exit(0);
}

/**
 * Função que retorna o tipo de content-type
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
 * Função para montar um cabeçalho de resposta padrão HTTP
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

    /* Preparando o tipo de conteúdo */
	sprintf(buffer, "Content-Type: %s\r\n", mimeType);
	write(client_socket, buffer, strlen(buffer));

    /* Preparando o tamanho */
	if (tamanho >= 0) {
		sprintf(buffer, "Content-Length: %d\r\n", tamanho);
		write(client_socket, buffer, strlen(buffer));
	}

    /* Preparando a data/hora da última modificação para efeitos de cache */
	if (ultAtu != -1){
		strftime(bufHora, sizeof(bufHora), RFC1123, gmtime(&ultAtu));
		sprintf(buffer, "Last-Modified: %s\r\n", bufHora);
		write(client_socket, buffer, strlen(buffer));
   	}

    /* Enviando linha em branco para finalizar o cabeçalho */
	write(client_socket, "\r\n", 2);
}

/**
 * Função que envia página de erro para o cliente
 */
int returnErro(int client_socket, char *mensagem, int request_check) {
	char buffer[100];

    /* Preparando cabeçalho de resposta */
    if (request_check==404) {
        httpHeader(client_socket,"text/html",-1,-1, request_check, "Pagina nao encontrada");
    }
    else if (request_check==501) {
        httpHeader(client_socket,"text/html",-1,-1, request_check, "Metodo nao implementado");
    }
    else if (request_check==403) {
        httpHeader(client_socket,"text/html",-1,-1, request_check, "Forbidden");
    }

    /* Montando a página de erro */
	sprintf(buffer, "<html>\n<head>\n<title>%s : Erro %d</title>\n</head>\n\n", SERVERNAME,request_check);
	write(client_socket, buffer, strlen(buffer));

	sprintf(buffer, "<body>\n<h1>%s : Erro %d</h1>\n", SERVERNAME,request_check);
	write(client_socket, buffer, strlen(buffer));

	sprintf(buffer, "<p>%s</p>\n</body>\n</html>\n",mensagem);
	write(client_socket, buffer, strlen(buffer));

	return 0;
}

/**
 * Função para mensagens de erro no console
 */
void saidaErro(char msg[100]){
	fprintf(stderr, "%s: %s\r\n", SERVERNAME, msg);
	exit(EXIT_FAILURE);
}

/**
 * Função que verifica a Request solicitada pelo browser
 */
CR_returns checkRequest(Host client, Request client_request) {

	CR_returns returns;

	/*Guardar informações sobre o caminho do recurso*/
	char pathbuf[MAXBUF];

	/*Para auxliar na contagem de tamanho do "caminho"*/
	int len;

    /* Copiar para a variável dir o caminho do DIR_ROOT*/
	strcpy(returns.dir,DIR_ROOT);

	/*Se existir o recurso, concatena com a variavel dir, se nao complementa com a pagina inicial /index.html*/
	if(client_request.recurso)
		strcat(returns.dir,client_request.recurso);
	else strcat(returns.dir,"/index.html");

	printf("\n [Pasta]                      %s\n", returns.dir);
	printf("\n ************************************************************************\n\n");

	/* Se o tipo de método nao existir returns com codigo 501 */
	if((strcmp(client_request.metodo, "GET") != 0)&&(strcmp(client_request.metodo, "POST") != 0))
		{returns.answerCOD = 501; return returns;}

	/* Se o arquivo nao existe returns com check 404 */
	if(stat(returns.dir, &returns.statBuffer) == -1)
		{returns.answerCOD = 404; return returns;}

	/* Se o arquivo for encontrado e for um diretorio */
	if(S_ISDIR(returns.statBuffer.st_mode)) { //Inicio if 1
		len = strlen(returns.dir);

		/* Se não colocar a barra no final da url, no caso de ser um diretório*/
		if (len == 0 || returns.dir[len - 1] != '/'){ //Inicio if 2

			/* Adicionar a "/index.html" barra no final da URL(pelo menos dentro do servidor)*/
			snprintf(pathbuf, sizeof(pathbuf), "%s/", returns.dir);
			strcat(returns.dir,"/index.html");

			/* Se existir o index.html dentro do diretorio, então returns com check == 200, se nao existir check == 404*/
			if(stat(returns.dir, &returns.statBuffer) == -1)
				{returns.answerCOD = 404; return returns;}
			else
				{returns.answerCOD = 200; return returns;}
		}
		else{
			/* Se não tiver colocar "/" depois do nome do diretório então colocará apenas index.html*/
			snprintf(pathbuf, sizeof(pathbuf), "%sindex.html", returns.dir);
			strcat(returns.dir,"index.html");

			/* Se existir o index.html dentro do diretorio, então returns com answerCOD == 200, se nao existir answerCOD == 404*/
			if(stat(returns.dir, &returns.statBuffer) >= 0)
				{returns.answerCOD = 200; return returns;}
			else
				{returns.answerCOD = 404; return returns;}
		} //Fim if 2
	}
	/* Se for um arquivo que nao seja um diretorio returns com answerCOD == 200, com o endereço da url completo (statBuffer) */
	else
		{returns.answerCOD = 200; return returns;} //Fim if 1
}

/**
 *Função que recebe e organiza a requisição do cliente
 */
Request readRequest(Host client) {

    ssize_t  n;
	Request client_request;
	char line[MAXBUF];
	char *metodo = malloc(5);
	char *recurso = malloc(1000);
	char *protocolo = malloc(20);
	char *post = malloc(800);

    /*Lendo requisição do cliente*/
    n=read(client.socket, line, MAXBUF);

	/* Colocar os dados do socket(requisicao completa) na variável "line" */
	strcpy(client_request.line, line);

	/* Manipulando a variavel line para extrair dados como o método, recurso, protocolo e post*/
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
 * Função para enviar o arquivo
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
        /* Preparando cabeçalho de resposta */
		httpHeader(client_socket,get_mime_type(returns.dir),tamanho ,returns.statBuffer.st_mtime, returns.answerCOD, "OK");

        /*Enviando arquivo */
		while ((i = read(returns.n, &dados,1)))
			write(client_socket, &dados, 1);

	}
}

/**
 * Função que envia resposta a requisição GET do browser
 */
void sendGETRequest(Host client, CR_returns returns){
	if (returns.answerCOD == 501) // Método não suportado pelo servidor
	        returnErro(client.socket,"<h2>Metodo nao suportado</h2>", returns.answerCOD);

	if (returns.answerCOD == 404) // Arquivo não encontrado
	        returnErro(client.socket,"<h2>Pagina nao encontrada</h2>", returns.answerCOD);

    if (returns.answerCOD == 403) // Arquivo sem permissão de leitura/execução
	        returnErro(client.socket,"<h2>Forbidden</h2>", returns.answerCOD);

	if (returns.answerCOD == 200){ //Arquivo OK
	        returns.n = open(returns.dir, O_RDONLY);
	        sendFile(client.socket,returns);
	}
}

/**
 * Função que envia resposta a requisição POST do browser
 */
void sendPOSTRequest(Host client, CR_returns returns, Request client_request) {

	char buffer[100];
    if (returns.answerCOD == 501) // Método não suportado pelo servidor
	        returnErro(client.socket,"<h2>Metodo nao suportado</h2>", returns.answerCOD);

	if (returns.answerCOD == 404) // Arquivo não encontrado
	        returnErro(client.socket,"<h2>Pagina nao encontrada</h2>", returns.answerCOD);

    if (returns.answerCOD == 403) // Arquivo sem permissão de leitura/execução
	        returnErro(client.socket,"<h2>Forbidden</h2>", returns.answerCOD);

	if (returns.answerCOD == 200){ //Arquivo OK
	        /*Preparando os objetos posts*/
	        int cont = contaPosts(client_request.post);

	        /* Preparando cabeçalho de resposta */
            httpHeader(client.socket,"text/html",-1,-1, returns.answerCOD, "OK");

            /* Montando a página resultado de um POST */
            sprintf(buffer, "<html>\n<head>\n<title>Resultado do POST</title>\n</head>\n\n");
            write(client.socket, buffer, strlen(buffer));

            /* Imprimindo listagem de objetos */
            sprintf(buffer, "<body>\n<h2>Variáveis POST</h2><BR>\n");
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
                    sprintf(buffer, "Variável:  %s", tockens);
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
 * Função que retorna a última linha de uma requisição POST
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
 * Função que conta quantos objetos posts foram enviados ao servidor
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
