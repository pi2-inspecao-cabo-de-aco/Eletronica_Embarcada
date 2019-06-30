/* RICAS V3.0 - Software de controle do Robô Inspetor de Cabos de Aço em Serviço */
 

/* Bibliotecas */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <termios.h>

/*Variáveis Globais*/
pid_t pid[3];

/*Inicializando as Subrotinas*/
int com();
int le_comando();
void encerrar();
void init();
void fdc();
void tratamento_fdc_1();
void tratamento_fdc_2();
int rotina_manual(int comando, int j);
int rotina_auto(int j);
void mfl();
void cam();
void comp(int j);


/* Função Principal */
int main(int argc, char **argv){


	//Inicializando arquivos e GPIO's
	init();
	
	//Iniciando os processos 
	pid[0] = fork();
	
	//Processo 1: Comunicação HTTP
	if(pid[0] == 0){
	
		int retorno;
		char bff[30];
		
		while(1){
			printf("Processo 1: Comunicação HTTP, %d\n", getpid());
			retorno = com();
			sprintf(bff, "sudo echo \"%d\" > /tmp/com.txt", retorno);
			system(bff);
		}
		
	}else pid[1] = fork(); 
	
	//Processo 2: Movimentação e Sensoriamento
	if(pid[1] == 0){
	
		int flag_fdc=0, comando, j=0;
		
		signal(SIGUSR1, tratamento_fdc_1);
		signal(SIGUSR2, tratamento_fdc_2);
		printf("Processo 2: Movimentação e Sensoreamento, %d\n", getpid());
		
		while(1){
		
			comando = le_comando(j);
			if(comando == 4) j=rotina_auto(j);
			else if(comando == 0 || comando == 1 || comando == 2 || comando == 3) j=rotina_manual(comando, j); 
			else{ 
				printf("Aguardando instruções...\n");
				sleep(1);
			}
			printf("Quantidade de Passos = %d\n", j);
				
		}
		
	}else pid[2] = fork();
	
	//Processo 3: Sinal de Fim de Curso
	if(pid[2] == 0){
		printf("Processo 3: Sinal de Fim de Curso, %d\n", getpid());
	
		while(1){
			fdc();
			usleep(1000);
		}
	}
	
	//Processo Pai: Controle dos processos
	else{
		signal(SIGINT, encerrar);
		while(1) sleep(1);
	}
	
			
return 0;
}

/*Subrotinas:*/

//Função de inicialização
void init(){
 
	int fd;
	
	//Verifica a existência das pastas data e comp, onde ocorre o processamento dos dados coletados pelo totem. 
	fd = open("/tmp/data", O_RDONLY);
	close(fd);
	if(fd<0) system("mkdir /tmp/data");
	fd = open("/tmp/comp", O_RDONLY);
	close(fd);
	if(fd<0) system("mkdir /tmp/comp");
	
	//Verifica a existência dos arquivos mfl.txt e com.txt
	system("> /tmp/data/mfl.txt");
	system("> /tmp/com.txt");
	
	//Inicializa GPIO's 7 e 8 para leitura dos sensores de fim de curso.
	system("sudo echo 7 >> /sys/class/gpio/export");
	printf("export 7 ok\n");
	system("sudo echo 8 >> /sys/class/gpio/export");
	printf("export 8 ok\n");
	system("sudo echo in > /sys/class/gpio/gpio7/direction");
	printf("direction 7 ok\n"); 
	system("sudo echo in > /sys/class/gpio/gpio8/direction"); 
	printf("direction 8 ok\n");
	
	//Inicializa GPIO's 18, 23, 24 e 25 para controle do led das câmeras
	system("sudo echo 18 >> /sys/class/gpio/export");
	printf("export 18 ok\n");
	system("sudo echo 23 >> /sys/class/gpio/export");
	printf("export 23 ok\n");
	system("sudo echo 24 >> /sys/class/gpio/export");
	printf("export 24 ok\n");
	system("sudo echo 25 >> /sys/class/gpio/export");
	printf("export 25 ok\n");
	system("sudo echo out > /sys/class/gpio/gpio18/direction");
	printf("direction 18 ok\n"); 
	system("sudo echo out > /sys/class/gpio/gpio23/direction");
	printf("direction 23 ok\n");
	system("sudo echo out > /sys/class/gpio/gpio24/direction");
	printf("direction 24 ok\n");
	system("sudo echo out > /sys/class/gpio/gpio25/direction");
	printf("direction 25 ok\n");
	system("sudo echo 0 > /sys/class/gpio/gpio18/value");
	printf("value 18 ok\n"); 
	system("sudo echo 0 > /sys/class/gpio/gpio23/value");
	printf("value 23 ok\n");
	system("sudo echo 0 > /sys/class/gpio/gpio24/value");
	printf("value 24 ok\n");
	system("sudo echo 0 > /sys/class/gpio/gpio25/value");  
	printf("value 25 ok\n"); 
	
}

//Função do estado Comunicação
int com(){

	//Estrutura do socket para Cliente (Totem) e Servidor(Robô)
	struct sockaddr_in cliente, servidor;
	//Variável para definir o tamanho de cliente
	socklen_t cliente_len = sizeof(cliente);
	//Descritores de arquivos para identificar cliente e servidor 
	int fd_cliente, fd_servidor, yes=1, porta = 4242;
	//Buffer para armazenar e tratar as informações transferidas via socket
	char bff[5];
	int len_bff, retorno=0;
	
	//Criando socket IPV4
	if((fd_servidor = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		printf("Erro ao criar o socket do servidor...\n");
		exit(1);
	}
	
	//Definindo as propriedade do socket Servidor
	memset(&servidor, 0, sizeof(servidor)); // Zerando a estrutura de dados
	servidor.sin_family = AF_INET;
	servidor.sin_addr.s_addr = htonl(INADDR_ANY);
	servidor.sin_port = htons(porta);
	
	//Verificando se a porta esta disponível
    if((setsockopt(fd_servidor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) < 0){
        printf("Porta em uso! erro nas configurações do socket...\n");
        exit(1);
    }
	
	//Ligando o socket Servidor à http PORTA
	if((retorno = bind(fd_servidor, (struct sockaddr*) &servidor, sizeof(servidor))) < 0){
		printf("Erro ao ligar o socket do servidor com a porta (%d)... retorno = %d\n", porta, retorno);
		exit(1);
	}
	
	//Escutando a porta, e esperando por um cliente;
	if((listen(fd_servidor, 1)) < 0){
		printf("Erro a escutar a porta (%d)...\n", porta);
		exit(1);
	}
	
	//Aceitando a comunicação com o cliente
	if((fd_cliente = accept(fd_servidor,(struct sockaddr *) &cliente, &cliente_len)) < 0){
		printf("Erro ao aceitar o cliente ...\n");
		exit(1);
	}
	
	if((len_bff = recv(fd_cliente, bff, 5, 0)) > 0){
		bff[len_bff] = '\0';
		printf("Mensagem do cliente: %s \n", bff);	
		retorno = atoi(bff);
	}
		
	close(fd_cliente);
	close(fd_servidor);
	//printf("retorno = %d\n", retorno);
	
	return retorno;
			
}

int le_comando(int j){
	 
	 int fd;
	 char bff[30] ="5";
	 
	 if((fd = open("/tmp/com.txt", O_RDONLY)) < 0) printf("le_comando: Erro ao abrir o arquivo com.txt!\n");
	 if((read(fd, &bff, sizeof(bff))) < 0)printf("le_comando: Erro ao ler o arquivo com.txt");
	 close(fd);
	
	 return atoi(bff); 

}
	
int rotina_manual(int comando, int j){

	int uart0, i;
	struct termios config;
	
	//Abrindo a porta uart0
	if((uart0 = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY)) == -1) printf("Erro ao abrir a porta serial0\n");
	
	//Setando as configurações do UART
	tcgetattr(uart0, &config);
	config.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
	config.c_iflag = IGNPAR;
	config.c_oflag = 0;
	config.c_lflag = 0;
	tcflush(uart0, TCIFLUSH);
	tcsetattr(uart0, TCSANOW, &config);


	if(comando == 0 && j >= 0){ 
		printf("Stop ativado, voltando para posição inicial. \n");
		for(i=0; i<32; i++) if(write(uart0, "3", sizeof(char)) == -1) printf("Erro ao escrever na porta serial\n");
		j-=32;	
	}
	else if(comando == 1){ 
		printf("Pause ativado.\n");
		if(write(uart0, "1", sizeof(char)) == -1) printf("Erro ao escrever na porta serial\n");
	}
	else if(comando == 2){ 
		printf("Manual: Para frente!\n"); 
		if(write(uart0, "2", sizeof(char)) == -1) printf("Erro ao escrever na porta serial\n");
		j++;
	}
	else if(comando == 3 && j >= 0){ 
		printf("Manual: Para tras!\n"); 
		if(write(uart0, "3", sizeof(char)) == -1) printf("Erro ao escrever na porta serial\n");
		j--;
	}
	close(uart0);
	 
	return j;
}

int rotina_auto(int j){

	int i;

	for(i=0; i<32; i++){ 
		mfl();
		j++;
	}	
	cam();
	comp(j);
	
	return j;
}

void mfl(){

	int len_bff, fd, uart0;
	char bff[60];
	struct termios config;
	
	//Abrindo a porta uart0
	if((uart0 = open("/dev/serial0", O_RDWR | O_NOCTTY | O_NDELAY)) == -1) printf("Erro ao abrir a porta serial0\n");
	
	//Setando as configurações do UART
	tcgetattr(uart0, &config);
	config.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
	config.c_iflag = IGNPAR;
	config.c_oflag = 0;
	config.c_lflag = 0;
	tcflush(uart0, TCIFLUSH);
	tcsetattr(uart0, TCSANOW, &config);
	
	//Enviando via uart
	if(write(uart0, "4", sizeof(char)) == -1) printf("Erro ao escrever na porta serial\n");
	
	usleep(100000);
	
	if((len_bff = read(uart0, &bff, 60)) == -1) printf("Erro ao ler da porta serial\n");
	else{
		printf("%s", bff);
		if((fd = open("/tmp/data/mfl.txt", O_WRONLY | O_APPEND)) == -1) printf("Erro ao abrir o arquivo mfl.txt\n");
		bff[len_bff + 1] = '\0';
		if(write(fd, &bff, len_bff) == -1) printf("Erro ao escrever no arquivo mfl.txt\n");
		close(fd);
	}
	close(uart0);
}

void cam(){
	
	printf("Rotina de Cameras\n");
	system("sudo echo 1 > /sys/class/gpio/gpio18/value");
	system("sudo echo 1 > /sys/class/gpio/gpio23/value");
	system("sudo echo 1 > /sys/class/gpio/gpio25/value");  
	system("sudo fswebcam -d /dev/video0 -r 2592x1944 --no-banner /tmp/data/image1.jpg");
	system("sudo echo 0 > /sys/class/gpio/gpio25/value"); 
	system("sudo echo 1 > /sys/class/gpio/gpio24/value"); 
	system("sudo fswebcam -d /dev/video1 -r 2592x1944 --no-banner /tmp/data/image2.jpg");
	system("sudo echo 0 > /sys/class/gpio/gpio18/value"); 
	system("sudo echo 1 > /sys/class/gpio/gpio25/value");
	system("sudo fswebcam -d /dev/video2 -r 2592x1944 --no-banner /tmp/data/image3.jpg");
	system("sudo echo 0 > /sys/class/gpio/gpio23/value"); 
	system("sudo echo 1 > /sys/class/gpio/gpio18/value");
	system("sudo fswebcam -d /dev/video3 -r 2592x1944 --no-banner /tmp/data/image4.jpg"); 
	system("sudo echo 0 > /sys/class/gpio/gpio18/value");
	system("sudo echo 0 > /sys/class/gpio/gpio24/value");
	system("sudo echo 1 > /sys/class/gpio/gpio25/value");
	
}

void comp(int j){

	char buff[120];
	printf("Compilando arquivos da pasta /tmp/data !\n");
	sprintf(buff,"tar -cf %d.tar /tmp/data/ && mv %d.tar /tmp/comp", (j*5), (j*5)); 
	system(buff);
	sprintf(buff,"rm -R /tmp/data && mkdir /tmp/data && > /tmp/data/mfl.txt");
	system(buff);

}

//Função do estado Sensor FDC
void fdc(){

	int fd;
	char sinal1='0', sinal2='0'; 
		
	fd = open("/sys/class/gpio/gpio7/value", O_RDONLY);
	lseek(fd, 0, SEEK_SET);
	read(fd, &sinal1, sizeof(sinal1));
	close(fd);
	
	fd = open("/sys/class/gpio/gpio8/value", O_RDONLY);
	lseek(fd, 0, SEEK_SET);
	read(fd, &sinal2, sizeof(sinal2));
	close(fd);		
			
	//enviando o sinal SIGUSR1 para o processo de Sensoriamento e Movimentação (pid[1]).
	if(sinal1 == '0'){ 
		kill(pid[1], SIGUSR1);
		sleep(1);
	}
	if(sinal2 == '0'){ 
		kill(pid[1], SIGUSR2);
		sleep(1);
	}	 
}

void tratamento_fdc_1(){	
	printf("FDC frontal identificado... Acionando Stop ! \n");
	system("sudo echo \"0\" > /tmp/com.txt");
	
}

void tratamento_fdc_2(){
	printf("FDC traseiro identificado... Acionando Pause ! \n");
	system("sudo echo \"1\" > /tmp/com.txt");
}

//tratamento do sinal ^C
void encerrar(){ 
	printf("\nEncerrando processos...\n");
	kill(pid[0], SIGTERM);
	kill(pid[1], SIGTERM);
	kill(pid[2], SIGTERM);
	system("sudo echo 7 >> /sys/class/gpio/unexport");
	system("sudo echo 8 >> /sys/class/gpio/unexport");
	system("sudo echo 18 >> /sys/class/gpio/unexport");
	system("sudo echo 23 >> /sys/class/gpio/unexport");
	system("sudo echo 24 >> /sys/class/gpio/unexport");
	system("sudo echo 25 >> /sys/class/gpio/unexport");
	exit(0);
}


 
