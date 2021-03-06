#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define FALSE 0
#define TRUE 1
#define MESSAGESIZE 256
#define USERNAMESIZE 256
#define PORTNUMBER 4000
#define MAXCONNECTIONS 5

typedef struct
{
	int in;

} param;

typedef struct
{
	int online;
	int id;
	int socket;
	char* username;
	pthread_t thread;

} Client;

Client* clients;
int roomSize = 400;
int numClients;

pthread_attr_t pthread_custom_attr;

int sockfd;
struct sockaddr_in server_address;

pthread_t adminThread;
pthread_t acceptLoopThread;
pthread_t playMp3Thread;

int withSounds;
int chatIsOn;

int color = 31;
//char blue[20] = "\e[1;34m";
char normal[20] = "\e[0m";

void* readAdminCommands(void* args);
void* acceptLoop(void* args);
void* worker(void* args);
void initSocket();
void acceptConnection(int clientId);
void readUsername(int threadIndex);
void readMessages(int threadIndex);
void sendToAllClients(char* message);
void getTime(char* buf);
void toLower(char* destination, char* source);
void killClient(int clientId);
void* playMp3(void* args);
void installDependencies();

int main(int argc, char const *argv[])
{
	int i;

	if(argc == 2)
	{
		if(strcmp(argv[1],"sounds") == 0)
		{
			withSounds = TRUE;
			//installDependencies();
		}
		else
		{
			withSounds = FALSE;
		}
	}

	initSocket();

	// allocating memory for clients' array
	clients = (Client*)calloc(roomSize,sizeof(Client));

	// initializing clients
	for(i = 0; i < roomSize; i++)
	{
		clients[i].online = FALSE;
		clients[i].id = i;
		clients[i].username = (char*)calloc(USERNAMESIZE,sizeof(char));		
	}

	chatIsOn = TRUE;

	system("clear");
	fprintf(stderr,"Welcome! Your chat is on!\n");
	fprintf(stderr,"Type 'close' to end the chat\n\n");

	pthread_create(&adminThread, &pthread_custom_attr, readAdminCommands, (void*)0);
	pthread_create(&acceptLoopThread, &pthread_custom_attr, acceptLoop, (void*)0);

	while(chatIsOn) {}

	for(i = 0; i < numClients; i++)
	{
		if(clients[i].online)
			killClient(i);
	}

	close(sockfd);

	return 0;
}

void* readAdminCommands(void* args)
{
	char typingBuffer[100];

	bzero(typingBuffer,100);

	fprintf(stderr,"> ");

	fgets(typingBuffer,100,stdin);

	while(chatIsOn)
	{
		if(strcmp(typingBuffer,"close\n") == 0)
		{
			fprintf(stderr, "\nClosing chat...\n");
			chatIsOn = FALSE;
			fprintf(stderr, "Chat is offline. Come back later!\n");
		}
		else if(strcmp(typingBuffer,"pony\n") == 0)
		{
			if(withSounds)
			{
				pthread_create(&playMp3Thread, &pthread_custom_attr, playMp3, (void*)"sounds/pony.mp3");
			}

			fprintf(stderr, "\n");
		}
		else
		{
			fprintf(stderr,"Fuck you, this command is not recognized\n\n");
		}

		fprintf(stderr,"> ");
		fgets(typingBuffer,100,stdin);
	}

	fprintf(stderr, "Chat is offline. Come back later!\n");

	return;
}

void* acceptLoop(void* args)
{
	numClients = 0;

	while(chatIsOn)
	{
		numClients++;
		acceptConnection(numClients-1);
		pthread_create(&(clients[numClients-1].thread), &pthread_custom_attr, worker, &(clients[numClients-1].id));
	}
}

void* worker(void* args)
{
	param* p = (param*)args;
	int clientId = p->in;

	readUsername(clientId);

	char loginMessage[MESSAGESIZE], timeString[MESSAGESIZE];
	getTime(timeString);
	sprintf(loginMessage,"\n%s	>> %s is online <<\n\n",timeString, clients[clientId].username);
	sendToAllClients(loginMessage);

	if(withSounds)
	{
		pthread_create(&playMp3Thread, &pthread_custom_attr, playMp3, (void*)"sounds/greeting.mp3");
	}

	readMessages(clientId);
}

void initSocket()
{
	int yes = 1;
	
	// opening socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if(sockfd == -1)
	{
		fprintf(stderr, "ERROR opening socket\n");
		exit(0);
	}

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORTNUMBER);
	server_address.sin_addr.s_addr = INADDR_ANY;
	bzero(&(server_address.sin_zero), 8);

	// binding
	int could_bind = bind(sockfd, (struct sockaddr*) &server_address, sizeof(server_address)) >= 0;

	if(!could_bind)
	{
		fprintf(stderr, "ERROR on binding\n");
		exit(0);
	}	
}

void acceptConnection(int clientId)
{
	int newsockfd;
	socklen_t clientLength;
	struct sockaddr_in client_address;

	// listening
	listen(sockfd, MAXCONNECTIONS);

	clientLength = sizeof(struct sockaddr_in);

	// accepting
	newsockfd = accept(sockfd, (struct sockaddr*) &client_address, &clientLength);

	if(newsockfd == -1)
	{
		fprintf(stderr,"ERROR on accept\n");
		exit(0);
	}

	clients[clientId].socket = newsockfd;
	clients[clientId].online = TRUE;
}

void readUsername(int clientId)
{
	int n;
	char buffer[USERNAMESIZE];

	bzero(buffer, USERNAMESIZE);

	n = read(clients[clientId].socket, buffer, USERNAMESIZE);

	if(n < 0)
	{
		fprintf(stderr,"ERROR reading from socket\n");
		exit(0);
	}

	sprintf(clients[clientId].username,"%c[%d;%dm%s%c[%dm",27,1,color,buffer,27,0);

	color = 31 + (color - 31 + 1)%6;

	char timeString[80];

	getTime(timeString);
}

void readMessages(int clientId)
{
	int n;
	char message[MESSAGESIZE];

	while(chatIsOn)
	{
		char timeString[80];
		getTime(timeString);

		bzero(message, MESSAGESIZE);
	
		n = read(clients[clientId].socket, message, MESSAGESIZE);
	
		if(n < 0)
		{
			fprintf(stderr,"ERROR reading from socket\n");
			exit(0);
		}

		char* lowercaseMessage = (char*)calloc(strlen(message),sizeof(char));
		toLower(lowercaseMessage,message);

		if(strcmp(lowercaseMessage,"/logout") == 0)
		{
			char logoutMessage[MESSAGESIZE];
			sprintf(logoutMessage,"\n%s	>> %s is offline <<\n\n", timeString, clients[clientId].username);

			clients[clientId].online = FALSE;
			close(clients[clientId].socket);
			sendToAllClients(logoutMessage);

			return;
		}
		else if(strcmp(lowercaseMessage,"/buzz") == 0)
		{
			char buzzMessage[MESSAGESIZE];
			sprintf(buzzMessage,"\n%s	>> %s sent a buzz <<\n\n", timeString, clients[clientId].username);

			sendToAllClients(buzzMessage);

			if(withSounds)
			{
				pthread_create(&playMp3Thread, &pthread_custom_attr, playMp3, (void*)"sounds/buzz.mp3");
			}
		}
		else if(strcmp(lowercaseMessage, "/pony") == 0)
		{
			if(withSounds)
			{
				pthread_create(&playMp3Thread, &pthread_custom_attr, playMp3, (void*)"sounds/pony.mp3");
			}
		}
		else
		{
			char saysMessage[MESSAGESIZE];

			sprintf(saysMessage, "%s	%s says: %s\n", timeString, clients[clientId].username, message);
			sendToAllClients(saysMessage);

			if(withSounds)
			{
				pthread_create(&playMp3Thread, &pthread_custom_attr, playMp3, (void*)"sounds/message.mp3");
			}
		}
	}
}

void sendToAllClients(char* message)
{
	int i, n;

	for(i = 0; i < roomSize; i++)
	{
		Client client = clients[i];

		if(client.online)
		{
			n = write(clients[i].socket, message, strlen(message));

			if(n < 0)
			{
				fprintf(stderr,"ERROR writing to socket");
				exit(0);
			}
		}
	}
}

void getTime(char* buf)
{
	time_t     now = time(0);
    struct tm  tstruct;
    tstruct = *localtime(&now);

    sprintf(buf,"%2d:%2d:%2d", tstruct.tm_hour, tstruct.tm_min, tstruct.tm_sec);
}

void toLower(char* destination, char* source)
{
	int i;

	for(i = 0; i < strlen(source); i++)
	{
		destination[i] = tolower(source[i]);
	}
}

void killClient(int clientId)
{
	clients[clientId].online = FALSE;

	int n = write(clients[clientId].socket, "KILL", strlen("KILL"));

	if(n < 0)
	{
		fprintf(stderr,"ERROR writing to socket");
		exit(0);
	}

	close(clients[clientId].socket);
}

void* playMp3(void* args)
{
	char* path = (char*)args;

	char command[50];
	sprintf(command,"mpg123 %s", path);

	system(command);
}

void installDependencies()
{

	system("sudo apt-get install mpg123");
}