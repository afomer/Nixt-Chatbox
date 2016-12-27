// Author: Julian Sam and Abubaker Omer
// Date Created: 17th December 2016
// Client side of the Chatbox application.
// GitHub: https://github.com/afomer/Nixt-Chatbox


/*
 _   _       _        _____ _           _    ______           
| | | |     (_)      /  __ \ |         | |   | ___ \          
| | | |_ __  ___  __ | /  \/ |__   __ _| |_  | |_/ / _____  __
| | | | '_ \| \ \/ / | |   | '_ \ / _` | __| | ___ \/ _ \ \/ /
| |_| | | | | |>  <  | \__/\ | | | (_| | |_  | |_/ / (_) >  < 
\___/|_| |_|_/_/\_\  \____/_| |_|\__,_|\__| \____/ \___/_/\_\
                 _____  _ _            _   
                /  __  \ (_)          | |  
                | /  \ / |_  ___ _ __ | |_ 
                | |    | | |/ _ \ '_ \| __|
                | \__/\| | |  __/ | | | |_ 
                \_____||_|_|____|_| |_|\__|
                
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
#define NUMCOLORSFORUSERNAME 5
#define RESPONSE_TIMEOUT 5

char* color_array[5] = {RED,GREEN,BLUE,CYAN,MAGENTA};

bool useTimeZone = false;
char myName[MAXLINE];
struct ChatBuffer
{
    char *chat;
    int serverfd;
};

struct ChatBuffer ClientChatBuff;
char name[MAXLINE];
int serverfd;

/* mutex for reading/writing */
sem_t CommandExecMutex;
sem_t FgetsMutex;
int ServerMsg;
char GlobalUserInput[MAXLINE];

///////////////////////////////////////////////////////////////////////////////
//////////////////////////// Function Definitions /////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void usage(void); // Prints info about app
void showCommands(void); // Show commands for user
void printUsers(int serverfd, char *buf); // Show all users
void startChat(int serverfd, rio_t rio_serverfd, char *name, char* typedName);
void JoinGroup(char* inputGroupName);

// Asking the user about a chat request
void ChatRequest(int serverfd, char *name, char *other_user); 
void ChatState(int serverfd, char *name, char *other_user);
void ReadingChatFromServer(void *ChatBuffer);
void ServerCommands(void *BufPtr);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//////////////////////    Time Functions   ////////////////////////////////////
char* returnWeekday(int x);
void trimTime(char* time);
char* getSurnameOfDate(char* date);
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////
////////////////////////////// Main Function  /////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Arg 1: IP
// Arg 2: Port #
int main(int argc, char **argv)
{
    char c;
    char buf[MAXLINE];
    serverfd = -1;
    sem_init(&CommandExecMutex, 0, 1);
    sem_init(&FgetsMutex, 0, 0);

    ServerMsg = 0;

    name[0] = 0; 
    char *envName = getenv("LOGNAME"); // Gets Username from env. variable
    strcat(name, envName);

    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////
    /////delete after testing////
    // appeding a random number to ur name
    time_t t;
    char str[50];

    /* Intializes random number generator */
    srand((unsigned) time(&t));
    sprintf(str, "%d", (rand() % 500));
    strcat(name,str);
    //////////////////////////////////////////////////////////////////////////
    
    if (argc != 3 || argv[1] == NULL || argv[2] == NULL) {
      fprintf(stderr, "usage: %s <server> <port>\n", argv[0]);
      exit(1);
    }


     strcpy(myName,name); // store name as global


    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////

    ////// Go through inputted flags /////////
    while ((c = getopt(argc, argv, "h")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
            break;
        default:
            usage();
        }
    }


    /****** Abubaker: Connecting to the Server ********/

    // Connect to the NIXT server the client requested 
    // (make sure they are not corrupted)
    serverfd = open_clientfd(argv[1], argv[2]);

    // if the domain couldn't be identinfied, 
    // write bad GET Request to the user
    if ( serverfd < 0 )
    {
        fprintf(stderr, "Could not connect to the server\n");
        exit(1);
    }
    
    // As a handshake between the server and the client, the name of the
    // user is sent to the server, first.
    rio_writen(serverfd, name, sizeof(name) + 1);

    /**************/

    
    // A chat buffer for recieving user input while using fgets
    ClientChatBuff.serverfd = serverfd;
    
    rio_t rio_serverfd;

    printf(GREEN "Welcome to the NIXT ChatBox, " RESET);
    printf(CYAN "%s \n" RESET, name);
    printf("Created By Abubaker Omer and Julian Sam \n");
    printf("Type " YELLOW "\"c\"" RESET " to see possible commands\n");
    
    usleep(500);
    
    pthread_t ServerMsgTid;
    
    pthread_create(&ServerMsgTid, NULL, (void *)ServerCommands, (void *)(&buf));

    while (1) {

        if (feof(stdin)) { /* End of file (ctrl-d) */
            printf ("\nGoodBye!\n");
            exit(0);
        }
        
        // Checking broken pipe
        if (errno == EPIPE)
        {
            printf("Lost Connection with the Chat Server\n");
            return 1;
        }

 
        /********************************************/
        printf(">> "); // Type Prompt Symbol 
        fflush(stdout); // Flush to screen        
        fgets (GlobalUserInput, MAXLINE, stdin); // Read command line input        
        sem_post(&FgetsMutex);

        sem_wait(&CommandExecMutex);
        if (ServerMsg)
        {
            ServerMsg = 0;
            sem_post(&CommandExecMutex);
            GlobalUserInput[0] = 0;
            continue;
        }

        if (!strcmp("c\n",GlobalUserInput))   
            showCommands();

        else if (!strcmp("exit\n",GlobalUserInput) 
                 || !strcmp("quit\n",GlobalUserInput)
                 || !strcmp("q\n",GlobalUserInput) )// Exit client
        {
            // Terminating the reading thread
            pthread_cancel(ServerMsgTid);
            rio_writen(serverfd,"exit",strlen("exit")+1);
            close(serverfd);
            exit(0);
        }
        else if (!strcmp("chat\n",GlobalUserInput)) 
        {   
            startChat(serverfd, rio_serverfd, name,NULL);
        }
        else if (strstr(GlobalUserInput,"chat "))
        {
            startChat(serverfd, rio_serverfd, name,GlobalUserInput+5);
        }
        else if (strstr(GlobalUserInput, "whois "))
        {
            printf("***Must have Finger Installed. Lookup on local machine.\n"); 
            printf("Type \"users\" to see users on the chat system.\n");
           
            char finger_GlobalUserInput[MAXLINE];
            finger_GlobalUserInput[0] = 0;

            strcat(finger_GlobalUserInput,"finger ");
            strcat(finger_GlobalUserInput, GlobalUserInput+6);
            system(finger_GlobalUserInput);
        }
        else if (!strcmp("group\n",GlobalUserInput) || !strcmp("grp\n",GlobalUserInput)) 
        {
            JoinGroup(NULL);
        }
        else if (strstr(GlobalUserInput,"group "))
        {
            JoinGroup(GlobalUserInput+6);
        }

        else if (!strcmp("groups\n",GlobalUserInput) || !strcmp("grps\n",GlobalUserInput)) 
        {
            printf("Group Chats:\n");
            printUsers(serverfd, GlobalUserInput);
        }
        else if (!strcmp("users\n",GlobalUserInput) )
        {
            printf("Online Users:\n");
            printUsers(serverfd, GlobalUserInput);
        }
        else if (!strcmp("clear\n", GlobalUserInput)) // clearing the screen
        {
            printf("\x1b[2J \033[0;0H");
        }
        else if (!strcmp("whoami\n",GlobalUserInput) )
            printf("Your Username: %s\n", name);
        else 
        {
            printf(RED "Invalid Command." RESET " Try Again. Type " YELLOW "\"c\" " RESET "to see commands \n");
        }

        GlobalUserInput[0] = 0;
        sem_post(&CommandExecMutex);

    }       
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////
////////////////////////// Server Functions ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ServerCommands(void *BufPtr)
{   
    char server_buf[MAXLINE],command[MAXLINE], 
    arg1[MAXLINE], arg2[MAXLINE];

    char **BufAddr = (char **)(BufPtr);

    int server_buf_not_empty = 0;
    while (true)
    {
        /******* reading data from the server *******/

        ioctl(serverfd, SIOCINQ, &server_buf_not_empty);
        
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
                char *logged_in = "This user is already logged in NIXT Chat Server";
                printf("%s \n", logged_in );
                close(serverfd);
                exit(0);
            }
        }

        sem_post(&CommandExecMutex);

    }
}

void startChat(int serverfd, rio_t rio_serverfd, char *name, char* typedName)
{
    char other_user[MAXLINE], // name of the user the client want to chat with
    chatrqst_instr[MAXLINE], buf[MAXLINE];
    
    if (typedName)
    {
        strcpy(other_user,typedName);
    }
    else
    {
        printf("Enter Name/ID: ");
        fflush(stdout);
        fgets(other_user, MAXLINE, stdin); // Read command line input   
    }

    // other_user string inherently have a '\n', because it's entered by the user
    // we will copy it to a new variable to remove that \n
    // (stripped from '\n')
    int other_user_len = strlen(other_user);
    char other_user_stripped[other_user_len];
    memcpy(other_user_stripped, other_user, other_user_len);
    other_user_stripped[other_user_len - 1] = '\0';
    
    if(!strcmp(other_user_stripped, name))
    {
        printf("You Can't Chat With Yourself :] \n");
        return;
    } 

    printf("Waiting For %s's Response... \n", other_user_stripped );

    chatrqst_instr[0] = 0; 
    // Sending chat requests is like chatrqst <who you want to chat with>
    strcat(strcat(chatrqst_instr,"chatrqst "), other_user_stripped);
        
    // Asking the server, to invite the other user to join a chat
    rio_writen(serverfd, chatrqst_instr, strlen(chatrqst_instr) + 1);
        
    // Wait for the user response, and timeout after 5 seconds
    int other_client_responded = 0;
    ioctl(serverfd, SIOCINQ, &other_client_responded);
    
    while ( !other_client_responded)
    {
        ioctl(serverfd, SIOCINQ, &other_client_responded);
    }

    
    read(serverfd, buf, MAXLINE);
        
    char command[MAXLINE], arg1[MAXLINE], arg2[MAXLINE];
    printf("c:%s \n", command );

    sscanf(buf,"%s %s %s",command, arg1, arg2);
    
    if (!strcmp("offline", command) || !strcmp("offline\n", command))
    {
        printf(RED "%s " RESET "is " BLUE "offline" RESET "/" GREEN "unavailable\n" RESET, other_user_stripped);
        return;
    }
    else if (!strcmp("chatting", command) || !strcmp("chatting", command) )
    {
        printf("%s is busy/chatting with someone else =] \n", other_user_stripped);
        return;
    }
    else if (!strcmp("rejected", command))
    {
        printf(RED "%s " RESET GREEN "Rejected your Chat invitation \n" RESET , arg1);
        return;
    }

    // The other user accepted the chat invitation
    printf( BLUE "%s " RESET RED "Accepted " RESET GREEN "the Chat invitation!\n"RESET, arg1);

    // send to the server to be in a ChatState
    char JoinChat_instruct[MAXLINE];
    
    JoinChat_instruct[0] = 0; 

    /* Telling your thread in the server-side to be in a ChatState*/
    // join a chat a 1-to-1 chat, joinchat <other user name>
    strcat(strcat(JoinChat_instruct,"joinchat 1to1 "), other_user_stripped);
        
    sleep(1);
    
    // Asking the server, to invite the other user to join a chat
    rio_writen(serverfd, JoinChat_instruct, strlen(JoinChat_instruct) + 1);

    ChatState(serverfd, name, other_user_stripped);

    return;
}



void ChatRequest(int serverfd, char *user, char *other_user)
{   
    char buf[MAXLINE];
    char other_user_response[MAXLINE];
    
    // tell client about the user that want to chat with him/her
    printf("\x1b[K\r"); // erase the line and start from the beggining
    printf("%s wants to chat, do you accept? [Y/N] ", other_user);
    fflush(stdout);
    
    sem_wait(&FgetsMutex);
    strcpy(buf, GlobalUserInput);
    strcpy(GlobalUserInput, "");


    while(strcmp(buf,"n\n") && strcmp(buf,"N\n") 
        && strcmp(buf,"Y\n") && strcmp(buf,"y\n")
        && strcmp(buf,"Yes\n") && strcmp(buf,"yes\n")
        && strcmp(buf,"No\n") && strcmp(buf,"no\n"))
    {
       buf[0] = 0; 
       printf("\nPlease answer with y or n. ");
       fflush(stdout);
       fgets(buf, MAXLINE, stdin); // Read command line input
    }
    
    other_user_response[0] = 0; 

    if (!strcmp(buf,"n\n") || !strcmp(buf,"N\n") ||
        !strcmp(buf,"no\n") || !strcmp(buf,"No\n"))
    {   
        // chatansno , answering no for a chat request (Chat Answer No)
        strcat(strcat(other_user_response, "chatansno "), other_user);
        rio_writen(serverfd, other_user_response, strlen(other_user_response) + 1);
        printf( RED "Rejected " RESET BLUE "Chat invitation from " RESET GREEN "%s\n" RESET, other_user);
        return;
    }
    else
    {
        // chatansyes , answering yes for a chat request (Chat Answer yes)
        strcat(strcat(other_user_response, "chatansyes "), other_user);
        rio_writen(serverfd, other_user_response, strlen(other_user_response) + 1);
        ChatState(serverfd, user, other_user);
    }

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
    char* myColor = color_array[rand()%NUMCOLORSFORUSERNAME];

    time_t rawtime;
    char* splitString;
    char* day;
    char* date;
    char* timeOfDay;
    char* month;

    strcpy(user_text_buf,"");

    printf( RED "Joined " RESET "Chat with "GREEN "%s" RESET "\n", other_user);

    /* Spawning a thread to read messages directly from the server and prints them
       to the client's screen directly as they are recieved */
    ClientChatBuff.chat = user_text_buf;
    
    pthread_t tid;
    pthread_create(&tid, NULL, (void *)ReadingChatFromServer, (void *)(&ClientChatBuff));
    
    // <my_name> <space> <msg>
    meta_info_buf[0] = 0;


    time( &rawtime );
    splitString = ctime(&rawtime);

    // Split string to get proper time format.
    day   = strtok(splitString," ");
    month = strtok(NULL," ");
    (void)month; // TO avoid compiler issues.
    date  = strtok(NULL," ");
    timeOfDay  = strtok(NULL," ");
    trimTime(timeOfDay);

    sprintf(meta_info_buf, YELLOW "[%s-%s%s-%s]%s" RED" %s " RESET "%s", 
            day,date,getSurnameOfDate(date),timeOfDay, myColor, client,"Joined The Chat!\n");

    rio_writen(serverfd, meta_info_buf, strlen(meta_info_buf) + 1);

    printf("\x1b[K\r>> "); // erase line and start writing


    // Reading client' messages until exit command
    while (strcmp("exit\n", user_text_buf) && strcmp("q\n", user_text_buf)
                    && !feof(stdin))
    { 
        fgets (user_text_buf, MAXLINE, stdin); // Read command line input
        printf("\x1b[A\x1b[K\r");// delete your line from the terminal
        
        // <my_name> <space> <msg>
        meta_info_buf[0] = 0;


        time( &rawtime );
        splitString = ctime(&rawtime);

        // Split string to get proper time format.
        day   = strtok(splitString," ");
        month = strtok(NULL," ");
        (void)month; // TO avoid compiler issues.
        date  = strtok(NULL," ");
        timeOfDay  = strtok(NULL," ");
        trimTime(timeOfDay);

        // Before exiting the chat announce it 
        if (!strcmp("exit\n", user_text_buf) || !strcmp("q\n", user_text_buf))
        {
            // <usr> Exited The Chat
            sprintf(meta_info_buf, YELLOW "[%s-%s%s-%s]%s" RED" %s " RESET "%s", 
                day,date,getSurnameOfDate(date),timeOfDay, myColor, client,"Exited The Chat\n"); 
        }
        else if (!strcmp("clear\n", user_text_buf))
        {
            printf("\x1b[2J \033[0;0H");
        }
        else if (!strcmp("list\n", user_text_buf) || !strcmp("users\n", user_text_buf))
        {   
            //printf("%s\n", buf );
            printUsers(serverfd, user_text_buf);
            printf(">> ");
            continue;
        }
        else
        {
            sprintf(meta_info_buf, YELLOW "[%s-%s%s-%s]%s %s: " RESET "%s", 
                    day,date,getSurnameOfDate(date),timeOfDay, myColor, client, user_text_buf);

        }

        rio_writen(serverfd, meta_info_buf, strlen(meta_info_buf) + 1);
    }

    // Terminating the reading thread
    pthread_cancel(tid);

    rio_writen(serverfd,"exit",strlen("exit")+1);

    printf(RED "Exited " RESET "Chat with " GREEN "%s" RESET "\n", other_user);

    return;
}



// NOTICE: THIS IS INTENDED TO BE AN INFINITE LOOP,
// AND NEED TO BE TERMINARTED BY THE MAIN thread
void ReadingChatFromServer(void *ChatBuffer)
{   
    struct ChatBuffer *ClientChatStruct = (struct ChatBuffer *)ChatBuffer;
    int serverfd = ClientChatStruct->serverfd;
    //char **ClientTyped = &(ClientChatStruct->chat);
    char server_buf[MAXLINE];
    int server_buf_not_empty;
    
    while (true)
    { 
        /// Recieving Other participants Chat, if there is
        ioctl(serverfd, SIOCINQ, &server_buf_not_empty);
        
        if ( server_buf_not_empty )
        {   
            read(serverfd, server_buf, MAXLINE);
            // print the chat msg starting from the beginning of the line
            printf("\n\r\x1b[K\x1b[A\r\x1b[K"); // erase 2 line and start writing
            printf("%s", server_buf); 
            printf(">> "); // rewrite the deleted user input
            fflush(stdout);
        }
    }

    return;
}

/*
 * JoinGroup - Join a group chat, or make one if you want to
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
                printf(RED "Created group " RESET BLUE "%s !" RESET "\n", GroupNameStripped);
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
    rio_writen(serverfd, buf, sizeof(buf));

    // Read Server Response
    read(serverfd, users, MAXLINE);

    // Print response
    printf("%s", users);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


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
    printf(YELLOW "   \"whois" GREEN " <name>\" " RESET " : Lookup user with username" GREEN " <name>" RESET " on Local Machine\n");
    printf(YELLOW "   \"exit\" " RESET " : Exit\n");
    return;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
///////////////////////////// Time Functions; /////////////////////////////////
char* returnWeekday(int x)
{
    char *weekday = calloc(sizeof(char),15);
    if (x == 0)
        strcpy(weekday,"Sun");
    else if (x == 1)
        strcpy(weekday,"Mon");
    else if (x == 2)
        strcpy(weekday,"Tue");
    else if (x == 3)
        strcpy(weekday,"Wed");
    else if (x == 4)
        strcpy(weekday,"Thu");
    else if (x == 5)
        strcpy(weekday,"Fri");
    else if (x == 6)
        strcpy(weekday,"Sat");
    else
    {
        fprintf(stderr, "%s\n", "Error in Weekday No.");
        return NULL;
    }
    return weekday;
}
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
void trimTime(char* time)
{
    int len = strlen(time);
    time[len-3] = '\0';
    return;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////