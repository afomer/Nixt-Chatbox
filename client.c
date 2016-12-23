
// Created by: Julian Sam on 17th December 2016.

/*
 _   _       _        _____ _           _    ______           
| | | |     (_)      /  __ \ |         | |   | ___ \          
| | | |_ __  ___  __ | /  \/ |__   __ _| |_  | |_/ / _____  __
| | | | '_ \| \ \/ / | |   | '_ \ / _` | __| | ___ \/ _ \ \/ /
| |_| | | | | |>  <  | \__/\ | | | (_| | |_  | |_/ / (_) >  < 
 \___/|_| |_|_/_/\_\  \____/_| |_|\__,_|\__| \____/ \___/_/\_\
*/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include "curl/curl.h"
#include "util.h"


// Color Escape Codes
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m" // Reset color code


void usage(void); // Prints info about app
void showCommands(void); // Show commands for user
void printUsers(int serverfd, char *buf); // Show all users
void startChat(int serverfd, rio_t rio_serverfd, char *name);
void startGroup();
void printHelpInfo(); // prints help info: add later
void ChatRequest(int serverfd, char *name, char *other_user); // Asking the user about a chat request
void ChatState(int serverfd, char *name, char *other_user);
void PrintCurrentTime();
void ReadingChatFromServer();
void HandleServerRequest();

// Global Variable for Server File Descriptor
int serverfd;

/* Semaphore Lock for handling server requests, that need client's answer */
sem_t handle_server_request;
int server_handled;
char name[MAXLINE];


// SSH Timezone Functions
struct string_holder;
void init_string(struct string_holder *s); 
size_t writefunc(void *ptr, size_t size, size_t nmemb, 
										struct string_holder *s);
char* getTimeZone(char* IP);

int main(int argc, char **argv)
{
	char c;
    bool SSH;
    char buf[MAXLINE];
	char* timeString = NULL;
	char* splitString = NULL;
	char* IPname;
    char *name2 = getenv("LOGNAME"); // Gets Username from env. variable
    strcat(name, name2);
    
    // initializing the semaphore. Read before you get user input
    sem_init(&handle_server_request, 0, 1);

    /////delete after testing////
    // appeding a random number to ur name
    time_t t;
    char str[50];

   /* Intializes random number generator */
   srand((unsigned) time(&t));
   sprintf(str, "%d", (rand() % 500));
   strcat(name,str);
   /////
    
    if (argc != 3 || argv[1] == NULL || argv[2] == NULL) {
      fprintf(stderr, "usage: %s <server> <port>\n", argv[0]);
      exit(1);
    }
	// Variables to get local time
	time_t rawtime;
	struct tm * timeinfo;

	// Variables to get original time info
	char* timezone = NULL;
	char buffer[1000];
	FILE *pipe;
	int len; 
	/////////////////////////////////


	while ((c = getopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
            break;
        default:
            usage();
        }
    }


    // GET IP ADDRESS AND SET SSH BOOL IF USER HAS SSH'ed
	IPname = getenv("SSH_CLIENT"); 
	if (IPname == NULL) {
		SSH = false;
	}
	else {
		SSH = true;
		// Find timezone if ssh'ed based on original client IP
		splitString = getTimeZone(IPname);

		// Split SSH string to get only the necessary parts.
		timezone = "Asia/Qatar";
	}

	/****** Abubaker: Connecting to the Server ********/

	// Connect to the NIXT server the client requested 
	// (make sure they are not corrupted)
    serverfd = open_clientfd(argv[1], argv[2]);

    // if the domain couldn't be identinfied, 
    // write bad GET Request to the user
    if ( serverfd < 0 )
    {
    	printf("Could not connect to the Server \n");
    	return 1;
    }
    
    // As a handshake between the server and the client, the name of the
    // user is sent to the server, first.
    rio_writen(serverfd, name, sizeof(name) + 1);

	/**************/
    char server_buf[MAXLINE],command[MAXLINE], 
    arg1[MAXLINE], arg2[MAXLINE];
    
    rio_t rio_serverfd;
	int server_buf_not_empty;
	server_handled = 0;

    printf(GREEN "Welcome to the NIXT ChatBox, " RESET);
    printf(CYAN "%s \n" RESET, name);
    printf("Created By Abubaker Omer and Julian Sam \n");
    printf("Type " YELLOW "\"c\"" RESET " to see possible commands\n");
    

    // Recieving requests and messages from the server in a different thread
    /* Spawning a thread to read messages directly from the server and prints them
       to the client's screen directly as they are recieved */
    pthread_t tid;
    pthread_create(&tid, NULL, (void *)HandleServerRequest, NULL);
    
    usleep(500);

	while (1) {

		if (feof(stdin)) { /* End of file (ctrl-d) */
			printf ("\nGoodBye!\n");
		    exit(0);
		}
        
        // P(handle_server_request)
        // if it's a server request it will handled by HandleServerRequest thread
        sem_wait(&handle_server_request);

		printf("prompt >> "); // Type Prompt Symbol 
		
		fflush(stdout); // Flush to screen

		fgets (buf, MAXLINE, stdin); // Read command line input
                
		if (strlen(buf) == 2 && buf[0] == 'c') // Show commands
			showCommands();

		else if ((!strcmp("exit\n",buf) 
			     || !strcmp("quit\n",buf)
				 || !strcmp("q\n",buf)) )// Exit client
		{
			if (SSH == true) // Free malloc'ed strings before exiting.
			{
			}
			
			// Terminating the reading thread
            pthread_cancel(tid);    
			exit(0);
		}

		else if (!strcmp("chat\n",buf)) 
			startChat(serverfd, rio_serverfd, name);

		else if ( !strcmp("group\n",buf)) 
			startGroup();

		else if ( !strcmp("users\n",buf) )
			printUsers(serverfd, buf);
		else if (!strcmp("whoami\n",buf) )
			printf("%s\n", name);        

	}

		
		
}

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hva]\n");
    printf("   -h   print this message\n");
    printf("   -a   print information about this app\n");
    exit(1);
}

void showCommands(void)
{
    printf("User Commands:\n");
    printf(YELLOW "   \"users\"" RESET " : Show users on this machine\n");
    printf(YELLOW "   \"chat\" " RESET " : Start a chat with a user\n");
    printf(YELLOW "   \"group\"" RESET " : Start a group chat\n");
    printf(YELLOW "   \"exit\" " RESET " : Exit\n");
    return;
}

void printUsers(int serverfd, char *buf)
{   
	char users[MAXLINE];

    rio_writen(serverfd, buf, sizeof(buf));

    read(serverfd, users, MAXLINE);

    printf("%s", users);
}

void startChat(int serverfd, rio_t rio_serverfd, char *name)
{
	char other_user[MAXLINE], // name of the user the client want to chat with
	chatrqst_instr[MAXLINE], buf[MAXLINE];
	printf("Enter Name/ID: ");
	fflush(stdout);
	fgets (other_user, MAXLINE, stdin); // Read command line input
    
    // Sending chat requests is like chatrqst <who you want to chat with>
    strcat(strcat(chatrqst_instr,"chatrqst "), other_user);
        
    // Asking the server, to invite the other user to join a chat
    rio_writen(serverfd, chatrqst_instr, sizeof(chatrqst_instr));
        
    // Wait for the user response
    read(serverfd, buf, MAXLINE);
        
    char command[MAXLINE],arg1[MAXLINE], arg2[MAXLINE];

    sscanf(buf,"%s %s %s",command, arg1, arg2);
    
    if (!strcmp("offline", command))
    {
    	printf("The User is offline/unavailable\n");
    	return;
    }
    else if (!strcmp("rejected", command))
    {
    	printf("%s rejected the chat invitation.\n", arg1);
    	return;
    }
    // send to the server to be in a ChatState
    char JoinChat_instruct[MAXLINE];
    
    /* Telling your thread in the server-side to be in a ChatState*/
    // join a chat a 1-to-1 chat, joinchat <other user name>
    strcat(strcat(JoinChat_instruct,"joinchat 1to1 "), other_user);
        
    sleep(1);
    
    // Asking the server, to invite the other user to join a chat
    rio_writen(serverfd, JoinChat_instruct, strlen(JoinChat_instruct) + 1);

    ChatState(serverfd, name, other_user);


	// if (!lookup(name)) {
	// 	printf("User Not Found\n"); 
	// }
	
	// else {
	
	// 	printf("Starting Chat\n")
	
	/*  Code for chatting */
	
	// }

}
void startGroup()
{

	printf("Group Chat Started?\n");
}

void printHelpInfo()
{
	printf("Chat stuff here bro\n");
}

void ChatRequest(int serverfd, char *user, char *other_user)
{   
	char buf[MAXLINE];
	char *other_user_response = calloc(sizeof(char), MAXLINE);
    
    // tell client about the user that want to chat with him/her
	printf("%s Want to chat, do you want to ? [Y/N] ", other_user);
	
	fflush(stdout);
	
	printf("GETTING it1 \n");

	fgets (buf, MAXLINE, stdin); // Read command line input
    
    printf("GETTING it \n");

	while(strcmp(buf,"n\n") && strcmp(buf,"N\n") 
		&& strcmp(buf,"Y\n") && strcmp(buf,"y\n"))
	{
	   printf("Please answer with y or n. ");
	   fflush(stdout);
	   fgets (buf, MAXLINE, stdin); // Read command line input
	}

	if (!strcmp(buf,"n\n") || !strcmp(buf,"N\n"))
	{   
		// chatansno , answering no for a chat request (Chat Answer No)
		strcat(strcat(other_user_response, "chatansno "), other_user);
		rio_writen(serverfd, other_user_response, sizeof(other_user_response));
		return;
	}
	else
	{
		// chatansyes , answering yes for a chat request (Chat Answer yes)
		strcat(strcat(other_user_response, "chatansyes "), other_user);
		rio_writen(serverfd, other_user_response, strlen(other_user_response) + 1);
		ChatState(serverfd, user, other_user);
	}
    
    printf("SENT\n");
	free(other_user_response);

	return;

}
/*
 * * * * * * * * * * * * * * * * * * * * * *
 * ChatState:
 * The state where the client' thread in 
 * the server-side is recieving chat messages 
 * and forwarding them to the client, and also, what 
 * the client is typing is send to the the client'
 * thread and then sent to every user in the chat                          
 * * * * * * * * * * * * * * * * * * * * * *
 */
void ChatState(int serverfd, char *client, char *other_user)
{
    char user_text_buf[MAXLINE];

    printf("Joined Chat with %s ", other_user);

    
    /* Spawning a thread to read messages directly from the server and prints them
       to the client's screen directly as they are recieved */
    pthread_t tid;
    pthread_create(&tid, NULL, (void *)ReadingChatFromServer, (void *)(long *)(&serverfd));
    

    // Reading client' messages until he issue an exit command
   // printf(">> ");     // Prompt
    fgets(user_text_buf, MAXLINE, stdin);

	while (strcmp("exit\n", user_text_buf))
	{ 
	  rio_writen(serverfd, user_text_buf, MAXLINE);
	//  printf(">> ");
	  fgets (user_text_buf, MAXLINE, stdin); // Read command line input
	}

	// Terminating the reading thread
    pthread_cancel(tid);

	rio_writen(serverfd,"exit",strlen("exit")+1);

	printf("exited chat\n");

	return;
}

// NOTICE: THIS IS INTENDED TO BE AN INFINITE LOOP,
// AND NEED TO BE TERMINARTED BY THE MAIN THREAD
void ReadingChatFromServer()
{
    char server_buf[MAXLINE];
    int server_buf_not_empty;
	
	while (true)
	{ 
	  
	  /// Recieving Other participants Chat, if there is
	  ioctl(serverfd, SIOCINQ, &server_buf_not_empty);
        
      if ( server_buf_not_empty )
       {   
	       read(serverfd, server_buf, MAXLINE);
	       printf(">> %s", server_buf );

	   }

	}

	return;
}

void HandleServerRequest()
{   
    char command[MAXLINE],arg1[MAXLINE], arg2[MAXLINE];
	char server_buf[MAXLINE];
	int server_buf_not_empty = 0;
    
    while (true)
    {
		ioctl(serverfd, SIOCINQ, &server_buf_not_empty);

		if ( server_buf_not_empty )
		{   
		    read(serverfd, server_buf, MAXLINE);

		    sscanf(server_buf,"%s %s %s", command, arg1, arg2);
			
		    if (!strcmp(command, "chatans"))
		    {

		       // Asking the user about a chat request
		       ChatRequest(serverfd, name, arg1);

		    }
		    else if (!strcmp(command,"quit"))
		    {
		    	printf("The Server is Shutting Down ...\n" );
		    	close(serverfd);
		    	exit(0);
		    }
		    else if (!strcmp(command,"logged"))
		    {
		    	char *logged_in = "This user is already logged in NIXT Chat Server";
		        printf("%s \n", logged_in );
		        close(serverfd);
		        exit(0);
		    }

	        sem_post(&handle_server_request);

		}
	
	  //printf("resume..\n");
	
	}
	

}

void PrintCurrentTime()
{


}




//////////////////////////////////////////////////////////////////
/////////////////// Get the Location of SSH IP ///////////////////


/*
 _____ _                                      _   _     _              
|_   _(_)                                    | | | |   (_)             
  | |  _ _ __ ___   ___ _______  _ __   ___  | | | |___ _ _ __   __ _  
  | | | | '_ ` _ \ / _ \_  / _ \| '_ \ / _ \ | | | / __| | '_ \ / _` | 
  | | | | | | | | |  __// / (_) | | | |  __/ | |_| \__ \ | | | | (_| | 
  \_/ |_|_| |_| |_|\___/___\___/|_| |_|\___|  \___/|___/_|_| |_|\__, | 
                                                                 __/ | 
                                                                |___/                      
 							_____            _ 
							/  __ \          | |
							| /  \/_   _ _ __| |
							| |   | | | | '__| |
							| \__/\ |_| | |  | |
							 \____/\__,_|_|  |_|
							                    
*///////////////////////////////////////////////////////////////


// Struct to hold string from libcurl
struct string_holder {
  char *ptr;
  size_t len;
};
// Initialize the string , malloc on heap.
void init_string(struct string_holder *s) {
  s->len = 0;
  s->ptr = malloc(s->len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}
// Write to string
size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string_holder *s)
{
  size_t new_len = s->len + size*nmemb;
  s->ptr = realloc(s->ptr, new_len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }
  memcpy(s->ptr+s->len, ptr, size*nmemb);
  s->ptr[new_len] = '\0';
  s->len = new_len;

  return size*nmemb;
}
// Use curl to get string
char* getTimeZone(char* IP)
{

  
  return NULL;
}
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////