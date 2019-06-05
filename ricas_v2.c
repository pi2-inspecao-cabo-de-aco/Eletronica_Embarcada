/* RICAS V2.0 - Software de controle do Robô Inspetor de Cabos de Aço em Serviço */
/*
Brasília, 25 de abril de 2019

		Esse software foi desenvolvido para rodar em um Raspberry pi 3B, rodando o SO Raspbian [1] e configurada como um AcessPoint em NAT [2].
		Tem o intuito de controlar as rotinas de Inicialização, Comunicação, Sensoriamento, Processamento de dados e Movimentação do autônomo inspetor de cabos desenvolvido na disciplina de Projeto Integrador 2 da Universidade de Brasília, Campus Gama.
		Desenvolvido pelo aluno de Engenharia Eletrônica, Leonardo Brandão (leonardobbfga@gmail.com)
		
		[1] https://www.raspberrypi.org/downloads/raspbian/
		[2] https://www.raspberrypi.org/documentation/configuration/wireless/access-point.md
		 
*/

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

/* Variável global */
pid_t pid[3];

/*Inicializando as Subrotinas*/

int init();
int ver();
int comp(int tempo_inicial);
int com(int fd_pipe[2]);
int cam();
int mfl();
int mov(int direcao, int fd_pipe[2]);
void fdc();
void parada();
void compilando();
void encerrar();

/* Função Principal */
int main(int argc, char **argv){

	int estado_atual=0; 
	int	proximo_estado=0;
	int tempo_inicial;
	int fd_pipe[2];
	
	if(argc<2){
		printf("Falta argumento !\n");
		printf("Execute: ./ricas 0, 1, 2 ou 3\n");
		printf("\t0: Processo 0 ativado: Sensoriamento e Movimentação \n");
		printf("\t1: Processo 1 ativado: Processamento de dados\n");
		printf("\t2: Processo 2 ativado: Sensor de Fim de Curso\n");
		printf("\t3: Todos os processos ativados\n\n");
		exit(0);
	} 
	
	//Marco Inicial de tempo
	tempo_inicial = init();
	
	//Pipe para comunicação entre processos
	pipe(fd_pipe);
	
	//Iniciando os processos 
	pid[0] = fork();
	
	//Processo Filho 0: Sensoriamento e Movimentação.
	//Estados: Comunicação (com), Movimentação (mov), Câmeras (cam) e Sensor MFL (mfl)
	if(pid[0] == 0){
		printf("Iniciando Processo Filho 0: Sensoriamento e Movimentação (pid = %d)\n", getpid());
		signal(SIGTERM, SIG_DFL);//Tratando o sinal de terminação
		signal(SIGUSR1, parada); //Tratando o sinal do sensor fdc
		signal(SIGUSR2, compilando); //Tratando sinal da função comp
		//Máquina de estados
		//0 -> stop/reset
		//1 -> pause
		//2 -> Manual frente
		//3 -> Manual tras
		//4 -> modo automático/iniciar    
		while(1){
			estado_atual = proximo_estado;
			if(estado_atual == 0) proximo_estado = com(fd_pipe);
			else if(estado_atual == 1 || estado_atual == 2) proximo_estado = mov(estado_atual, fd_pipe);
			else if(estado_atual == 3) proximo_estado = mfl();
			else if(estado_atual == 4) proximo_estado = cam();
		}
	}else pid[1] = fork(); 
		
	//Processo Filho 1: Processamento de dados. 
	//Estados: Verificar (ver), Compilar (comp)
	if(pid[1] == 0){
		
		//Tratando o sinal de terminação
		signal(SIGTERM, SIG_DFL);
		
		printf("Iniciando Processo Filho 1: Processamento de dados (pid = %d)\n", getpid());
	
		//Máquina de estados
		while(1){
			estado_atual = proximo_estado;
			if(estado_atual == 0) proximo_estado = ver();
			else if(estado_atual == 1) proximo_estado = comp(tempo_inicial);

		}
		
	}else pid[2] = fork();
	
	//Processo Filho 2: Sensor Fim de Curso
	//Estados: Sensor FDC (fdc)
	if(pid[2] == 0){
		printf("Iniciando Processo Filho 2: Sensor FDC (pid = %d)\n", getpid());
		signal(SIGTERM, SIG_DFL);//Tratando o sinal de terminação
		//Estado único
		while(1) fdc(pid[0]);
	}
	
	//Processo Pai: Controle dos processos
	else{
	
		int i;
	
		//Tratamento do sinal ^C
		signal(SIGINT, encerrar);
	
		//Flag de execução dos processos
		i = atoi(argv[1]);
		if((i == 1) || (i == 2)) kill(pid[0], SIGTERM);
		if((i == 0) || (i == 2)) kill(pid[1], SIGTERM);
		if((i == 0) || (i == 1)) kill(pid[2], SIGTERM);
			
		while(1) sleep(1);
		
	}
		
return 0;
}

/*Subrotinas:*/

int init(){
 
	int fd;
	
	//Verifica a existência das pastas data e comp, onde ocorre o processamento dos dados coletados pelo totem. 
	fd = open("/tmp/data", O_RDONLY);
	close(fd);
	if(fd<0) system("mkdir /tmp/data");
	fd = open("/tmp/comp", O_RDONLY);
	close(fd);
	if(fd<0) system("mkdir /tmp/comp");
	
	//Inicializa GPIO4 para leitura do sensor de fim de curso.
	system("sudo echo 4 >> /sys/class/gpio/export");//GPIO4 habilitada
	system("sudo echo in > /sys/class/gpio/gpio4/direction"); //GPIO4 como entrada
	
	return time(NULL);
}

//Função do estado Verificar
int ver(){

	int fd;
	int n_data;
	char bff;
	
	sleep(1);
	system("ls /tmp/data > /tmp/verificador.txt");
	system("wc -l /tmp/verificador.txt > /tmp/ver.txt && sudo chmod 777 /tmp/ver.txt");
	fd = open("/tmp/ver.txt", O_RDONLY);
	lseek(fd, 0, SEEK_SET);
	read(fd, &bff, sizeof(char));
	close(fd);
	n_data = bff - '0';
	printf("n_data = %d\n", n_data);
	
	//Condição para proximo estado
	if(n_data >= 8 && (n_data%2) == 0) return 1;
	else return 0;
	
}

//Função do estado Compilar
int comp(int tempo_inicial){

	int tempo_comp;
	char buff[120];
	
	tempo_comp = time(NULL);
	tempo_comp = tempo_comp - tempo_inicial;
	printf("tempo_comp = %d\n", tempo_comp);
	
	printf("Compilando arquivos da pasta /tmp/data !\n");
	
	kill(pid[0], SIGUSR2); //Avisa para o processo 0 que a pasta data será compilada e atualizada, logo não esta disponivel para uso.
	
	sprintf(buff,"tar -cf %d.tar /tmp/data/ && mv %d.tar /tmp/comp && rm -R /tmp/data && mkdir /tmp/data", tempo_comp, tempo_comp);
	system(buff);
	return 0;
}

//Função do estado Comunicação
int com(int fd_pipe){

	//Estrutura do socket para Cliente (Totem) e Servidor(Robô)
	struct sockaddr_in cliente, servidor;
	//Variável para definir o tamanho de cliente
	socklen_t cliente_len = sizeof(cliente);
	//Descritores de arquivos para identificar cliente e servidor 
	int fd_cliente, fd_servidor, yes=1, porta = 4242;
	//Buffer para armazenar e tratar as informações transferidas via socket
	char bff[5];
	int len_bff;
	int retorno=0;
	
	
	//Criando socket IPV4
	if((fd_servidor = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		printf("Erro ao criar o socket do servidor...\n");
		encerrar();
	}else printf("Socket IPV4 criado com sucesso !\n");
	
	//Definindo as propriedade do socket Servidor
	memset(&servidor, 0, sizeof(servidor)); // Zerando a estrutura de dados
	servidor.sin_family = AF_INET;
	servidor.sin_addr.s_addr = htonl(INADDR_ANY);
	servidor.sin_port = htons(porta);
	//memset(servidor.sin_zero, 0x0, 8); 
	
	//Verificando se a porta esta disponível
    if((setsockopt(fd_servidor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) < 0){
        printf("Porta em uso! erro nas configurações do socket...\n");
        encerrar();
    }else printf("Porta %d está disponivel\n", porta);
	
	//Ligando o socket Servidor à http PORTA
	if((retorno = bind(fd_servidor, (struct sockaddr*) &servidor, sizeof(servidor))) < 0){
		printf("Erro ao ligar o socket do servidor com a porta (%d)... retorno = %d\n", porta, retorno);
		encerrar();
	}else printf("Socket ligado a porta html (%d)\n", porta);
	
	//Escutando a porta, e esperando por um cliente;
	if((listen(fd_servidor, 1)) < 0){
		printf("Erro a escutar a porta (%d)...\n", porta);
		encerrar();
	}else printf("Socket escutando por novos clientes!\n");
	
	//Aceitando a comunicação com o cliente
	if((fd_cliente = accept(fd_servidor,(struct sockaddr *) &cliente, &cliente_len)) < 0){
		printf("Erro ao aceitar o cliente ...\n");
		encerrar();
	}else printf("Servidor aceitou o cliente: %d\n", fd_cliente);
	
	//Limpando a memoria do buffer
	//memset(bff, 0x0, 5);
	
	if((len_bff = recv(fd_cliente, bff, 5, 0)) > 0){
		bff[len_bff] = '\0';
		printf("Mensagem do cliente: %s \n", bff);
		retorno = atoi(bff);	
	}
		
	close(fd_cliente);
	close(fd_servidor);
	printf("retorno = %d\n", retorno);
	//Tratando o sinal recebido do totem
	if(retorno > 3) retorno = 0; 
	if else(retono == 3){ 
		if((write(fd_pipe[1], '1', 1)) < 0){ //Flag de rotina automática enviada para o estado de mov
			printf("Erro ao escrever no pipe!\n");
		}
	} 
	if else(retorno == 1 || retorno == 2){ 
		if((write(fd_pipe[1], '0', 1)) < 0){ //Flag de rotina manual enviada para o estado de mov
			printf("Erro ao escrever no pipe!\n");
		}
	}
	return retorno;
	
	
}

//Função do estado Movimentação
int mov(int direcao, int fd_pipe[2]){

	char bff;
	
	if(read(fd_pipe[0], bff, 1) < 0){
		printf("Erro ao ler o pipe!\n");
		return 0;
	}
	
	printf("Flag_auto = %c\n", bff);
	
	if(bff == '1'){
		
	}

	if(direcao == 1) printf("Movendo-se para frente\n");
	if(direcao == 2) printf("Movendo-se para tras\n");
}

//Função do estado Câmeras
int cam(){
	printf("Rotina de Cameras\n");
	system("cd /tmp/data && sudo fswebcam -d /dev/video0 -r 2592x1944 --no-banner image1.jpg && sudo fswebcam -d /dev/video1 -r 2592x1944 --no-banner image2.jpg && sudo fswebcam -d /dev/video2 -r 2592x1944 --no-banner image3.jpg && sudo fswebcam -d /dev/video3 -r 2592x1944 --no-banner image4.jpg");
	return 0;
}

//Função do estado Sensor MFL
int mfl(){

}

//Função do estado Sensor FDC
void fdc(){

	int fd;
	char sinal='0'; 
		
	fd = open("/sys/class/gpio/gpio4/value", O_RDONLY);
	lseek(fd, 0, SEEK_SET);
	read(fd, &sinal, sizeof(sinal));
	close(fd);		
			
	//enviando o sinal SIGUSR1 para o processo de Sensoriamento e Movimentação (pid[1]).
	if(sinal == '0') kill(pid[0], SIGUSR1);
		 
}

//Tratamento do sinal de parada.
void parada(){
	printf("Borda de Fim de curso detectada\n");
	usleep(500);
} 

//tramaneto do sinal de compilando
void compilando(){
	
}

//tratamento do sinal ^C
void encerrar(){ 
	printf("\nEncerrando processos...\n");
	kill(pid[0], SIGTERM);
	kill(pid[1], SIGTERM);
	kill(pid[2], SIGTERM);
	system("sudo echo 4 >> /sys/class/gpio/unexport");//GPIO4 desabilitada
	exit(1);
}

