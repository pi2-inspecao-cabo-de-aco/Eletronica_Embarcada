/* RICAS V1.0 - Software de controle do Robô Inspetor de Cabos de Aço em Serviço */
/*
Brasília, 25 de abril de 2019

		Esse software foi desenvolvido para rodar em um Raspberry pi 3B, rodando o SO Raspbian [1] e configurada como um AcessPoint em NAT [2].
		Tem o intuito de controlar as rotinas de Inicialização, Comunicação, Sensoriamento, Processamento de dados e Movimentação (MOV) do autônomo inspetor de cabos desenvolvido na disciplina de Projeto Integrador 2 da Universidade de Brasília, Campus Gama.
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
#include <pthread.h>
#include <signal.h>
#include <time.h>

/*Variáveis Globais*/

int flag_auto=0; //flag da rotina automática
int flag_fdc=0; //flag do sensor de fim de curso
int estado_atual=0; //descritor de estados
int proximo_estado=0; //descritor de estados


/*Inicializando as Subrotinas*/

void init(int i;);
int ver();
int comp();
int env();
int com();
int cam();
int mfl();
int mov();
void fdc();
void parada();
void compilando();
void encerrar();

/* Função Principal */
int main(){

	signal(SIGINT, encerrar);
	init(0);

return 0;
}

/*Subrotinas:*/

// Função de incilização dos processos
void init(int i){

	int fd;
	pid_t pid[2];
	
	//Verificando a existência da pasta data, onde ocorre o processamento dos dados coletados. 
	fd = open("/tmp/data", O_RDONLY);
	close(fd);
	if(fd<0) system("mkdir /tmp/data");
	
	/*//Inicilaizando GPIO4 para leitura do sensor de fim de curso.
	system("echo 4 >> /sys/class/gpio/export");//GPIO4 habilitada
	system("sudo echo in > /sys/class/gpio/gpio4/direction"); //GPIO4 como entrada
	system("sudo chmod 777 /sys/class/gpio/gpio4/value"); //Concedendo permissão máxima ao arquivo value
	*/
	//Iniciando os processos 
	pid[0] = fork();
	
	//Processo Filho 0: Processamento de dados. 
	//Estados: Verificar (ver), Compilar (comp) e Enviar (env)
	//Sinais: n_data 
	if(pid[0] == 0){
	printf("Iniciando processo Filho 0 (pid = %d)\n", getpid());
		//Máquina de estados
		while(i == 0 || i == 3){
			estado_atual = proximo_estado;
			if(estado_atual == 0) proximo_estado = ver();
			else if(estado_atual == 1) proximo_estado = comp(pid[1], i);
			else if(estado_atual == 2) proximo_estado = env();
			sleep(1);
		} 
	}else pid[1] = fork();
	
	//Processo Filho 1: Sensoriamento e Movimentação.
	//Estados: Comunicação (com), Movimentação (mov), Câmeras (cam) e Sensor MFL (mfl)
	//Sinais: USR[3] e Auto 
	if(pid[1] == 0){
	printf("Iniciando processo Filho 1 (pid = %d)\n", getpid());
		//tratando o sinal do sensor fdc
		signal(SIGUSR1, parada); 
		signal(SIGUSR2, compilando);
		//Máquina de estados
		while(i == 1 || i == 3){
			estado_atual = proximo_estado;
			if(estado_atual == 0) proximo_estado = com();
			else if(estado_atual == 1) proximo_estado = mov();
			else if(estado_atual == 2) proximo_estado = cam();
			else if(estado_atual == 3) proximo_estado = mfl();
		} 
	}
		
	//Processo Pai: Sensor Fim de Curso
	//Estados: Sensor FDC (fdc)
	else{
	printf("Iniciando processo Pai (pid = %d)\n", getpid());
		//Máquida de estados
		while(i == 2 || i == 3) fdc(pid[1]);
	
	} 
		
}

//Função do estado Verificar
int ver(){

	int fd;
	int n_data;
	char bff;
	
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
int comp(pid_t pid, int i){
	printf("Compilando arquivos da pasta /tmp/data !\n");
	if(i==3) kill(pid, SIGUSR2); //Avisa para o processo filho 1 que a pasta data será compilada e atualizada, logo não esta disponivel para uso.
	system("cd /tmp && tar -cf pacote.tar data/ && rm -R /tmp/data && mkdir /tmp/data");
	return 2;
}

//Função do estado Enviar
int env(){
	printf("Enviando dados para o Totem...\n");
	return 0;
}

//Função do estado Comunicação
int com(){

	
}

//Função do estado Movimentação
int mov(){
}

//Função do estado Câmeras
int cam(){
}

//Função do estado Sensor MFL
int mfl(){
}

//Função do estado Sensor FDC
void fdc(pid_t pid){

	int fd;
	char sinal='0'; 
		
	fd = open("/sys/class/gpio/gpio4/value", O_RDONLY);
	lseek(fd, 0, SEEK_SET);
	read(fd, &sinal, sizeof(sinal));
	close(fd);		
			
	//enviando o sinal SIGUSR1 para o processo de Sensoriamento e Movimentação (pid[1]).
	if(sinal == '0') kill(pid, SIGUSR1); 

}

//Tratamento do sinal de parada.
void parada(){

} 

//tramaneto do sinal de compilando
void compilando(){
	
}

//tratamento do sinal SIGINT
void encerrar(){
exit(0);
}

