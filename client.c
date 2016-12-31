/*

 ******************************************************************************
 *                                   client.c                                 *
 *                         The client of the NIXT-Chatbox                     *
 *                          Julian Sam & Abubaker Omer                        *
 *                       Date Created: 17th December 2016                     *
 *                     GitHub: https://github.com/afomer/Nixt-Chatbox         *
 *                                                                            *
 *  ************************************************************************  *
 *                               DOCUMENTATION                                *
 *                                                                            *
 *  ** General Walkthough  **                                                 *
 *                                                                            *
 *  The Server initially, after establishing a conncetion with the client,    *
 *  recieves the name of the user. After that the username is stored in       *
 *  the array of online users. After that commands are waited from the client *
 *  to either send, recieve msgs or join group chats.                         *
 *                                                                            *
 *  UPCOMING...                                                               *
 *                                                                            *
 *  ** Assigning usernames **                                                 *
 *                                                                            *
 *  ** Server Lookup for available usernames **                               *
 *                                                                            *
 *                                                                            *
 *  ************************************************************************  *

*/

/*
****************************************************************************
---------------------------- Library headers -------------------------------
****************************************************************************
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
#include "libs/util.h"

/*
****************************************************************************
----------------------------------------------------------------------------
****************************************************************************
*/


/*
****************************************************************************
-------------------------------- Macros ------------------------------------
****************************************************************************
*/

/* Color Escape Codes */
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
/*The color code for reset color*/
#define RESET   "\x1b[0m" 
/*Number of colors available as username colors*/
#define NUMCOLORSFORUSERNAME 5
/*Seconds until timeout*/
#define RESPONSE_TIMEOUT 5

/*
****************************************************************************
----------------------------------------------------------------------------
****************************************************************************
*/

/*
****************************************************************************
--------------------------- Global Variables -------------------------------
****************************************************************************
*/

/*Global int to hold server's socket file descriptor */
int ServerMsg;

int serverfd;

char GlobalUserInput[MAXLINE];

/*Backup buffer to hold string of the current prompt typed message*/
char backup_buffer[MAXLINE];

/*Global var to hold your own name*/
char myName[MAXLINE];

/*Global variable to hold user's name plus number*/
char name[MAXLINE]; 

/*An array of strings for available colors for the user*/
char* color_array[5] = {RED,GREEN,BLUE,CYAN,MAGENTA};

/* mutexes for reading/writing */
sem_t CommandExecMutex;
sem_t FgetsMutex;


/*
****************************************************************************
----------------------------------------------------------------------------
****************************************************************************
*/

/*
****************************************************************************
-------------------------------- Structs -----------------------------------
****************************************************************************
*/

struct ChatBuffer
{
    char *chat;
    int serverfd;
};
struct ChatBuffer ClientChatBuff;

/*
****************************************************************************
----------------------------------------------------------------------------
****************************************************************************
*/

/*
****************************************************************************
-------------------------- Function Prototypes -----------------------------
****************************************************************************
*/

void usage(void); 
void showCommands(void); 
void printUsers(int serverfd, char *buf);
void startChat(int serverfd, rio_t rio_serverfd, char *name, char* typedName);
void JoinGroup(char* inputGroupName);
void ChatRequest(int serverfd, char *name, char *other_user); 
void ChatState(int serverfd, char *name, char *other_user);
void ReadingChatFromServer(void);
void ServerCommands(void);
char* fgets_with_saved_buffer(char *dst, int max);
void trimTime(char* time);          
char* getSurnameOfDate(char* date); 

/*
****************************************************************************
----------------------------------------------------------------------------
****************************************************************************
*/

/*
****************************************************************************
----------------------------- Signal Handlers -------------------------------
****************************************************************************
*/

/*
 * sigint_handler: 
 * a signal handler for SIGINT signals
 * most probably the user enter Ctrl-C
 */
void sigint_handler(int signal)
{
    rio_writen(serverfd,"exit",strlen("exit")+1);
    printf("\r--> GoodBye =] !\n");
    close(serverfd);
    exit(0);
}

/*
****************************************************************************
----------------------------------------------------------------------------
****************************************************************************
*/

/*
*********************************************
*********************************************
******** The Client's Main Function *********
*********************************************
*********************************************
*
* main: Initialize the different global variables,
* open a connection socket with the given IP and
* port number and loop through users commands.
*
*/
int main(int argc, char **argv)
{
    /* Flag holder char var to go through inputted flags */
    char flag_holder;

    /* Initialize robust I/O server file descriptor */
    rio_t rio_serverfd;

    /* Create thread variable for main server request thread */
    pthread_t ServerMsgTid;

    /* Set initial file descriptor to check for error in connecting later */
    serverfd = -1;

    ServerMsg = 0;

    /* Initialise the mutexes */
    sem_init(&CommandExecMutex, 0, 1);
    sem_init(&FgetsMutex, 0, 0);
    
    /* Copy username from env. variable and copy into the name char buffer. */
    sprintf(name, "%s", getenv("LOGNAME")); 


    /************************ Uncomment for debugging purposes ****************************/
    // let you login with the same login name, by appening a random number next to your name
    /* time_t t;
    char str[50];
    srand((unsigned) time(&t));
    sprintf(str, "%d", (rand() % 100));
    strcat(name,str); */
    /************************************************************************/

    /* Print error message if port number or IP is not inputted */
    if (argc != 3 || argv[1] == NULL || argv[2] == NULL) {
      fprintf(stderr, "usage: %s <server> <port>\n", argv[0]);
      exit(1);
    }

    /* store name into global variable */
    strcpy(myName,name); 

    /* Go through inputted flags */
    while ((flag_holder = getopt(argc, argv, "h")) != EOF) {
        switch (flag_holder) {
        case 'h':  /* print help message */
            usage();
            break;
        //** Add additional flags here  **//
        default:
            usage();
        }
    }

    /****** Connecting to the Server ********/

    /* Connect to the NIXT server the client requested 
       (make sure they are not corrupted) */
    serverfd = open_clientfd(argv[1], argv[2]);

   /* if the domain couldn't be identinfied, 
      write bad GET Request to the user*/
    if ( serverfd < 0 )
    {
        fprintf(stderr, "Could not connect to the server\n");
        exit(1);
    }

    /* Signal Handler for SIGINT */
    signal(SIGINT,  sigint_handler);   /* ctrl-c */

    /* As a handshake between the server and the client, the name of the
       user is sent to the server, first. */

    rio_writen(serverfd, name, strlen(name) + 1);

    /******************************************/
    
    /* Begin printing to screen for the user */
    printf(GREEN "Welcome to the NIXT ChatBox, " RESET);
    printf(CYAN "%s \n" RESET, name);
    printf("Created By Abubaker Omer and Julian Sam \n");
    printf("Type " YELLOW "\"c\"" RESET " to see possible commands\n");
    
    /* Add small timer to ensure server has enough time to get setup */
    usleep(500);
    
    /* Creaete thread used to give new chat requests*/
    pthread_create(&ServerMsgTid, NULL, (void *)ServerCommands, (void *)(NULL));

    while (1) {

        /* End of file (ctrl-d) */
        if (feof(stdin)) 
        { 
            printf ("\nGoodBye!\n");
            exit(0);
        }
        
        /* Checking broken pipe */
        if (errno == EPIPE)
        {
            printf("Lost Connection with the Chat Server\n");
            return 1;
        }
 
        /********************************************/
        printf(">> ");  /* Type Prompt Symbol */ 
        fflush(stdout); /* Flush to screen */        
        fgets(GlobalUserInput, MAXLINE, stdin); /* Read command line input */ 

        /* Unlock FgetsMutex to let the code in the ChatRequest use 
           user input for the current fgets */

        /* Lock CommandExecMutex until the user commands are executed */
        sem_post(&FgetsMutex);
        sem_wait(&CommandExecMutex);

        // if the command came from the server, it was handled by the 
        // ServerCommand thread. Thus, unlock the CommandExecMutex and 
        // go on to the next iteration, to skip going through all the 
        // following user commands
        if (ServerMsg)
        {
            ServerMsg = 0;
            sem_post(&CommandExecMutex);
            GlobalUserInput[0] = 0;
            continue;
        }

        /* Show available commands, if user inputs c */ 
        if (!strcmp("c\n",GlobalUserInput))   
            showCommands();

        /* Exit application if user commands it */
        else if (!strcmp("exit\n",GlobalUserInput) 
                 || !strcmp("quit\n",GlobalUserInput)
                 || !strcmp("q\n",GlobalUserInput) )// Exit client
        {
            /* Terminating the reading thread */
            pthread_cancel(ServerMsgTid);
            rio_writen(serverfd,"exit",strlen("exit")+1);
            close(serverfd);

            exit(0);
        }

        /* Start a chat */ 
        else if (!strcmp("chat\n",GlobalUserInput)) 
        {   
            startChat(serverfd, rio_serverfd, name, NULL);
        }

        /* Any of the single-spaced commands */
        else if (strstr(GlobalUserInput," ")) 
        {
            /* First command word*/
            char command_name[MAXLINE];
            /* Second command word*/
            char second_command[MAXLINE];
            
            /* Length of command*/
            int command_len; 

            /* Scan input */
            sscanf(GlobalUserInput,"%s %s\n", command_name, second_command);

            /* Start chat with given name */
            if (!strcmp(command_name,"chat"))
            {

                /*Adding a newline character to work with startChat function*/
                command_len = strlen(second_command);
                second_command[command_len] = '\n'; 
                second_command[command_len+1] = '\0';

                startChat(serverfd, rio_serverfd, name,second_command);
            }

            /*Lookup user*/ 
            else if (!strcmp(command_name,"whois"))
            {
                printf("***Must have Finger Installed. Lookup on local machine.\n"); 
                printf("***Type \"users\" to see users on the chat system.\n");
               
                char finger_GlobalUserInput[MAXLINE];
                finger_GlobalUserInput[0] = 0;

                strcat(finger_GlobalUserInput,"finger ");
                strcat(finger_GlobalUserInput, second_command);
                system(finger_GlobalUserInput);
            }

            /*Start group chat in one line*/
            else if (!strcmp(command_name,"group"))
            {
                /*Adding newline to be compatible with JoinGroup function*/
                command_len = strlen(second_command);
                second_command[command_len] = '\n'; 
                second_command[command_len+1] = '\0';


                JoinGroup(second_command);
            }

            /*Invalid command message*/
            else
                printf(RED "Invalid Command." RESET " Try Again. Type "
                        YELLOW "\"c\" " RESET "to see commands \n");

        
        }

        /*Start a group chat*/
        else if (!strcmp("group\n",GlobalUserInput) 
                 || !strcmp("grp\n",GlobalUserInput)) 
        {
            JoinGroup(NULL);
        }

        /* Print group chats that are online */
        else if (!strcmp("groups\n",GlobalUserInput) 
                 || !strcmp("grps\n",GlobalUserInput)) 
        {
            printf("Group Chats:\n");
            printUsers(serverfd, GlobalUserInput);
        }

        /* Print users that are online */
        else if (!strcmp("users\n",GlobalUserInput) )
        {
            printf("Online Users:\n");
            printUsers(serverfd, GlobalUserInput);
        }

       /* Clear the entire screen */
        else if (!strcmp("clear\n", GlobalUserInput)) 
        {
            printf("\x1b[2J \033[0;0H");
        }
            
        /* Display username infomation */
        else if (!strcmp("whoami\n",GlobalUserInput) )
            printf("Your Username: %s\n", name);
        
        /* Show invalid command message */
        else 
            printf(RED "Invalid Command." RESET " Try Again. Type " YELLOW 
                "\"c\" " RESET "to see commands \n");
        
        // Lock FgetsMutex, so if the ServerCommand thread ran first, it 
        // waits for fgets
        sem_wait(&FgetsMutex);
        
        // Unlock CommandExecMutex. So, ServerCommand can execute server 
        // it commands, if there is
        sem_post(&CommandExecMutex);

    }       
}
/*
----------------------------------------------------------------------------
*/


///////////////////////////////////////////////////////////////////////////////
////////////////////////// Server Functions ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/*
 * ServerCommands: Listining to server commands, built
 * to be a thread in an infinite loop.
 */
void ServerCommands(void)
{   
    char server_buf[MAXLINE],command[MAXLINE], 
    arg1[MAXLINE], arg2[MAXLINE];

    // keep checking the server socket, lock with the CommandExecMutex mutex 
    // and execute server commands when you recieve them
    int server_buf_not_empty = 0;
    while (true)
    {
        /******* reading data from the server *******/
        server_buf_not_empty = 0;

        ioctl(serverfd, SIOCINQ, &server_buf_not_empty);
        
        // make reading and executing server commands atomic (uninterrupted)
        // by using CommandExecMutex mutex
        sem_wait(&CommandExecMutex);

        if ( server_buf_not_empty )
        {
            ServerMsg = 1;

            read(serverfd, server_buf, MAXLINE);
            
            if (errno == EPIPE)
            {
                printf("Lost Connection with the Chat Server\n");
                exit(0);
            }

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
                char *logged_in = 
                "This user is already logged in NIXT Chat Server";
                
                printf("%s \n", logged_in );
                close(serverfd);
                exit(0);
            }
        }
        sem_post(&CommandExecMutex);
    }
}

/*
 * startChat: Starting a chat with an online user on the server
 * Please add comments for this function
 * Warning: Function takes typedName with a new line at the end.
 */
void startChat(int serverfd, rio_t rio_serverfd, char *name, char* typedName)
{
    /*Name of the user the client want to chat with */
    char other_user[MAXLINE],
    chatrqst_instr[MAXLINE], buf[MAXLINE];
    
    /* If user provided a name, just copy it to var.*/
    if (typedName)
    {
        strcpy(other_user,typedName);
    }
    /* Else request name from user */
    else
    {
        printf("Enter Name/ID: ");
        fflush(stdout);
        fgets(other_user, MAXLINE, stdin); // Read command line input   
    }

    /* Other_user string inherently have a '\n', because it's entered by 
       the user we will copy it to a new variable to remove that \n
       (stripped from '\n') */
    int other_user_len = strlen(other_user);
    char other_user_stripped[other_user_len];
    memcpy(other_user_stripped, other_user, other_user_len);
    other_user_stripped[other_user_len - 1] = '\0';
  
    /* Make sure user didnt enter their own name */
    if(!strcmp(other_user_stripped, name))
    {
        printf("You Can't Chat With Yourself :] \n");
        return;
    } 

    /* Wait until recieve server response from other client */
    printf("Waiting For %s's Response... \n", other_user_stripped );

    /* Empty chatrqst_instr buffer*/
    chatrqst_instr[0] = 0; 
    // Sending chat requests is like chatrqst <who you want to chat with>
    strcat(strcat(chatrqst_instr,"chatrqst "), other_user_stripped);
        
    // Asking the server, to invite the other user to join a chat
    rio_writen(serverfd, chatrqst_instr, strlen(chatrqst_instr) + 1);
        
    // Wait for the user response, and timeout after 5 seconds
    int other_client_responded = 0;

    /* wait until you recieve data at the server socket, then read them */
    ioctl(serverfd, SIOCINQ, &other_client_responded);
    
    while ( !other_client_responded)
    {
        ioctl(serverfd, SIOCINQ, &other_client_responded);
    }

    
    read(serverfd, buf, MAXLINE);
        
    char command[MAXLINE], arg1[MAXLINE], arg2[MAXLINE];

    sscanf(buf,"%s %s %s",command, arg1, arg2);
    
    if (!strcmp("offline", command) || !strcmp("offline\n", command))
    {
        printf(RED "%s " RESET "is " BLUE "offline" RESET
         "/" GREEN "unavailable\n" RESET, other_user_stripped);
        return;
    }
    else if (!strcmp("chatting", command) || !strcmp("chatting", command) )
    {
        printf("%s is busy/chatting with someone else =] \n"
              , other_user_stripped);
        return;
    }
    else if (!strcmp("rejected", command))
    {
        printf(RED "%s " RESET GREEN "Rejected your Chat invitation \n"
              RESET , arg1);
        return;
    }

    // The other user accepted the chat invitation
    printf( BLUE "%s " RESET RED "Accepted " RESET GREEN "the Chat invitation!\n"RESET, arg1);

    // send to the server to be in a ChatState
    char JoinChat_instruct[MAXLINE];
    
    JoinChat_instruct[0] = 0; 

    /* Telling your thread in the server-side to be in a ChatState */
    /* Join a chat a 1-to-1 chat, joinchat <other user name> */
    strcat(strcat(JoinChat_instruct,"joinchat 1to1 "), other_user_stripped);
        
    sleep(1);
    
    /* Asking the server, to invite the other user to join a chat */
    rio_writen(serverfd, JoinChat_instruct, strlen(JoinChat_instruct) + 1);

    ChatState(serverfd, name, other_user_stripped);

    return;
}

/*
 * ChatRequest: Recieving a chat request from another online user.
 * whether the client accepted or declined the chat request, let the other user know,
 * and go into a ChatState, if the client accepted.
 */
void ChatRequest(int serverfd, char *user, char *other_user)
{   
    char other_user_response[MAXLINE];
    

    /* Tell client about the user that want to chat with him/her */
    printf("\x1b[K\r"); /* Erase the line and start from the beggining */
    printf("%s wants to chat, do you accept? [Y/N] ", other_user);
    fflush(stdout);
    
    sem_wait(&FgetsMutex);
    
     /* Loop until users answers correctly */
    while(strcmp(GlobalUserInput,"n\n") && strcmp(GlobalUserInput,"N\n") 
        && strcmp(GlobalUserInput,"Y\n") && strcmp(GlobalUserInput,"y\n")
        && strcmp(GlobalUserInput,"Yes\n") && strcmp(GlobalUserInput,"yes\n")
        && strcmp(GlobalUserInput,"No\n") && strcmp(GlobalUserInput,"no\n"))
    {
       printf("\nPlease answer with y or n. ");
       fflush(stdout);
       fgets(GlobalUserInput, MAXLINE, stdin); // Read command line input
    }
        

    if (!strcmp(GlobalUserInput,"n\n") || !strcmp(GlobalUserInput,"N\n") ||
        !strcmp(GlobalUserInput,"no\n") || !strcmp(GlobalUserInput,"No\n"))
    {   
        /* chatansno , answering no for a chat request (Chat Answer No) */
        strcpy(other_user_response, "chatansno ");
        strcat(other_user_response, other_user);
        rio_writen(serverfd, other_user_response, strlen(other_user_response) + 1);
        printf( RED "Rejected " RESET BLUE "Chat invitation from " RESET 
                GREEN "%s\n" RESET, other_user);
        return;
    }
    else
    {

        /* chatansyes , answering yes for a chat request (Chat Answer yes) */
        strcpy(other_user_response, "chatansyes ");
        strcat(other_user_response, other_user);
        rio_writen(serverfd, other_user_response, strlen(other_user_response) + 1);
        ChatState(serverfd, user, other_user);
    }
    
    GlobalUserInput[0] = 0; 

    other_user_response[0] = 0; 
    
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
    char meta_info_buf[MAXLINE];
    pthread_t tid;

    /* Pick a random color for your username */
    char* myColor = color_array[rand()%NUMCOLORSFORUSERNAME];

    /* Variables used to print time */
    time_t rawtime;
    char* splitString;
    char* day;
    char* date;
    char* timeOfDay;
    char* month;

    /* Empty the string*/
    strcpy(user_text_buf,"");

    printf( RED "Joined " RESET "Chat with "GREEN "%s" RESET "\n", other_user);

    /* Spawning a thread to read messages directly from the server and prints 
       them to the client's screen directly as they are recieved */
    
    pthread_create(&tid, NULL, (void *)ReadingChatFromServer, (void *)(NULL));
    
    /* Emtpy the buffer */
    meta_info_buf[0] = 0;

    /* Get current time */
    time( &rawtime );
    splitString = ctime(&rawtime);

    /* Split string to get proper time format */
    day   = strtok(splitString," ");
    month = strtok(NULL," ");
    (void)month; /* Added here to avoid compiler issues. */
    date  = strtok(NULL," ");
    timeOfDay  = strtok(NULL," ");

    /* Remove seconds from the time */
    trimTime(timeOfDay);

    /* Input proper buffer format */
    sprintf(meta_info_buf, GREEN "[%s-%s%s-%s]%s" RED" %s " RESET "%s", 
            day,date,getSurnameOfDate(date),timeOfDay, myColor, client,
            "Joined The Chat!\n");

    /* Write the string to the serer */
    rio_writen(serverfd, meta_info_buf, strlen(meta_info_buf) + 1);

    /* erase line and start writing */
    printf("\x1b[K\r>> "); 

    /* Loop until exit command or control D */
    while (strcmp("exit\n", user_text_buf) && strcmp("q\n", user_text_buf)
                   && !feof(stdin))
    { 
        
        /* Read from the user using specialized fgets function, that saves
           the current typed buffer. */
        fgets_with_saved_buffer(user_text_buf, MAXLINE); 
    
        /* delete the line from the terminal */
        printf("\x1b[A\x1b[K\r");
        
        /* Emtpy the buffer */
        meta_info_buf[0] = 0;

        /* Get current time */
        time( &rawtime );
        splitString = ctime(&rawtime);

        /* Split string to get proper time format */
        day   = strtok(splitString," ");
        month = strtok(NULL," ");
        (void)month; // TO avoid compiler issues.
        date  = strtok(NULL," ");
        timeOfDay  = strtok(NULL," ");
        trimTime(timeOfDay);

        /* Before exiting the chat, print to user */ 
        if (!strcmp("exit\n", user_text_buf) || !strcmp("q\n", user_text_buf))
        {
            sprintf(meta_info_buf, YELLOW "[%s-%s%s-%s]%s" RED" %s " RESET "%s", 
                day,date,getSurnameOfDate(date),timeOfDay, myColor, client,
                "Exited The Chat\n"); 
        }
        /* Clear command to clear prompt */
        else if (!strcmp("clear\n", user_text_buf))
        {
            printf("\x1b[2J \033[0;0H");
            continue;
        }  
        /* List users in the chat */
        else if (!strcmp("list\n", user_text_buf) || !strcmp("users\n", user_text_buf))
        {   
            printUsers(serverfd, user_text_buf);
            printf(">> ");
            continue;
        }
        /* Input the message into the buffer */
        else
        {
            sprintf(meta_info_buf, YELLOW "[%s-%s%s-%s]%s %s: " RESET "%s", 
                    day,date,getSurnameOfDate(date),timeOfDay, myColor, 
                                                    client, user_text_buf);
        }

        /* Write to server */
        rio_writen(serverfd, meta_info_buf, strlen(meta_info_buf) + 1);
    }

    /* Terminating the reading thread */
    pthread_cancel(tid);

    /* Send exit message to server */
    rio_writen(serverfd,"exit",strlen("exit")+1);

    /* Print exit message */
    printf(RED "Exited " RESET "Chat with " GREEN "%s" RESET "\n", other_user);

    return;
}



/*
 * ReadingChatFromServer - Main Reading Function
 * Warning: Needs to be terminated by the main thread
 * THIS IS INTENDED TO BE AN INFINITE LOOP,
 */
void ReadingChatFromServer()
{   

    char server_buf[MAXLINE];
    int server_buf_not_empty;
    
    while (true)
    { 
        /* Recieving Other participants Chat, if there is */
        ioctl(serverfd, SIOCINQ, &server_buf_not_empty);
        
        if ( server_buf_not_empty )
        {   
            read(serverfd, server_buf, MAXLINE);
            
            /* Erase 2 line and start writing */
            printf("\n\r\x1b[K\x1b[A\r\x1b[K"); 
            printf("%s", server_buf); 
            printf(">> "); 

            /* Rewrite the deleted user input in case new message interupts */
            if (strcmp(backup_buffer,"\n") 
                && backup_buffer[strlen(backup_buffer)-1] != '\n')
                printf("%s", backup_buffer);

            /* Flush to screen */
            fflush(stdout);
        }
    }

    return;
}

/*
 * JoinGroup - Join a group chat, or make one if it's not created yet.
 */
void JoinGroup(char* inputGroupName)
{
    char GroupName[MAXLINE], // name of the user the client want to chat with
    GroupNameInstruc[MAXLINE], buf[MAXLINE];

    if (inputGroupName)
    {
        strcpy(GroupName,inputGroupName);
    }
    else
    {
        printf("Group Name/ID: ");
        fflush(stdout);
        fgets (GroupName, MAXLINE, stdin); // Read command line input
    }
 
    // GroupName string inherently have a '\n', because it's entered by the user
    // we will copy it to a new variable to remove that \n
    // (stripped from '\n')
    int GroupNameLen = strlen(GroupName);
    char GroupNameStripped[GroupNameLen];
    memcpy(GroupNameStripped, GroupName, GroupNameLen);
    GroupNameStripped[GroupNameLen - 1] = '\0';
    
    // Checking if the group exit on the server
    GroupNameInstruc[0] = 0; 
    // Sending chat requests is like chatrqst <who you want to chat with>
    strcat(strcat(GroupNameInstruc,"grpexist "), GroupName);
        
    // Asking the server, to invite the other user to join a chat
    rio_writen(serverfd, GroupNameInstruc, strlen(GroupNameInstruc) + 1);
    
    printf("Checking if the group exist...\n");
    
    int server_responded = 0;

    ioctl(serverfd, SIOCINQ, &server_responded);
   
    while ( !server_responded)
    {
        ioctl(serverfd, SIOCINQ, &server_responded);
    }
    
    read(serverfd, buf, MAXLINE);
        
    char command[MAXLINE], arg1[MAXLINE], arg2[MAXLINE], CreationRequest[MAXLINE];

    sscanf(buf,"%s %s %s",command, arg1, arg2);
    
    if (!strcmp("no", command) || !strcmp("no\n", command) )
    {
        printf("The Group does not exit, do you want to create it [y/n] ? ");
        
        fgets(CreationRequest, MAXLINE, stdin);      
        
        while (strcmp(CreationRequest,"n\n") && strcmp(CreationRequest,"N\n") 
           && strcmp(CreationRequest,"Y\n") && strcmp(CreationRequest,"y\n"))
        {
            CreationRequest[0] = 0; 
            printf("\nPlease answer with y or n. ");
            fflush(stdout);
            fgets(CreationRequest, MAXLINE, stdin); // Read command line input
        }

        if ( !strcmp(CreationRequest,"y\n") || !strcmp(CreationRequest,"Y\n") 
            || !strcmp(CreationRequest,"y") || !strcmp(CreationRequest,"Y"))
        {   

            // Tell the server to make a group and go into a chatstate
            buf[0] = 0;
            strcat(strcat(buf, "crtgrp "), GroupNameStripped);
            rio_writen(serverfd, buf, strlen(buf) + 1);

            ioctl(serverfd, SIOCINQ, &server_responded);

            while ( !server_responded )
            {
                ioctl(serverfd, SIOCINQ, &server_responded);
            }

            read(serverfd, buf, MAXLINE);
            
            sscanf(buf,"%s %s %s",command, arg1, arg2);

            if ( !strcmp("created", command) || !strcmp("created\n", command))
            {
                printf(RED "Created group " RESET BLUE "%s !" RESET "\n"
                    , GroupNameStripped);
            }
            else
            {
                printf("Some Error Occured While Creating The Group\n");
                return;
            }

        }
        else
            return;
    }

    // send to the server to be in a ChatState
    char JoinChat_instruct[MAXLINE];
    
    JoinChat_instruct[0] = 0; 

    /* Telling your thread in the server-side to be in a ChatState*/
    // join a chat a 1-to-1 chat, joinchat <other user name>
    strcat(strcat(JoinChat_instruct,"joinchat grp "), GroupName);

    // Asking the server, to invite the other user to join a chat
    rio_writen(serverfd, JoinChat_instruct, strlen(JoinChat_instruct) + 1);

    ChatState(serverfd, name, GroupNameStripped);

    return;
}

/*
 * printUsers - Prints list of users
 */
void printUsers(int serverfd, char *buf)
{   
    char users[MAXLINE];

    // Request Users to server
    rio_writen(serverfd, buf, strlen(buf) + 1);

    // Read Server Response
    read(serverfd, users, MAXLINE);

    // Print response
    printf("%s", users);
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////// Utility Functions //////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
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

/*
 * showCommands - Shows possible commands for client
 */
void showCommands(void)
{
    printf("User Commands:\n");
    
    printf(YELLOW "   \"users\"" RESET " : Show users on this machine\n");
    
    printf(YELLOW "   \"chat\" " RESET " : Start a chat with a user\n");
    
    printf(YELLOW "   \"chat" GREEN " <name>\" " RESET " : Start a chat with a user with username" GREEN " <name>\n");
    
    printf(YELLOW "   \"group\"" RESET " : Start a group chat\n");
    
    printf(YELLOW "   \"group" GREEN " <groupname>\" " RESET " : Enter group with groupname" GREEN " <groupname>\n");
    
    printf(YELLOW "   \"groups\"" RESET " : List open groups\n");
    
    printf(YELLOW "   \"whois" GREEN " <name>\" " RESET " :  Lookup user with username" GREEN " <name>" RESET " on Local Machine\n");
    
    printf(YELLOW "   \"exit\" " RESET " : Exit\n");
    return;
}


/*
 * fgets_with_saved_buffer - fgets, but with a backup buffer with it
 */
char* fgets_with_saved_buffer(char *dst, int max)
{
    int c, i;
    char *p;
    i = 0;

    /* Empty initialize the buffer */
    
    backup_buffer[0] = 0;
    /* get max bytes or upto a newline */
    for (p = dst, max--; max > 0; max--) {
        if ((c = getchar()) == EOF)
            break;
        *p++ = c;

        /* Add characters to extra saved buffer as well as null terminator*/
        /* Null terminator added in case of printing needs */
        backup_buffer[i] = c;
        backup_buffer[i+1] = '\0';

        i++;
        if (c == '\n')
            break;
    }

    /* Adding null terminator to end of string*/
    *p = 0;
    
    if (p == dst || c == EOF)
        return NULL;

    return (p);
}

///////////////////////////////////////////////////////////////////////////////
////////////////////////  Time Functions //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * getSurnameOfDate - Returns the th or nd at the end of the date
 */
char* getSurnameOfDate(char* date)
{
    if (!strcmp("01",date) || !strcmp("21",date) || !strcmp("31",date))
        return "st";
    else if (!strcmp("02",date) || !strcmp("22",date) || !strcmp("32",date))
        return "nd";
    else if (!strcmp("03",date) || !strcmp("23",date) || !strcmp("33",date))
        return "rd";
    else
        return "th";
}
/*
 * trimTime - Removes the seconds from a time string
 */
void trimTime(char* time)
{
    int len = strlen(time);
    time[len-3] = '\0';
    return;
}

///////////////////////////////////////////////////////////////////////////////
//////////////////////////////  The End   /////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
