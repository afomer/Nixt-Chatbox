// Author: Julian Sam
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

bool useTimeZone = false;
char myName[MAXLINE];

///////////////////////////////////////////////////////////////////////////////
//////////////////////////// Function Definitions /////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void usage(void); // Prints info about app
void showCommands(void); // Show commands for user
void printUsers(int serverfd, char *buf); // Show all users
void startChat(int serverfd, rio_t rio_serverfd, char *name);
void startGroup();
void printHelpInfo(); // prints help info: add later

// Asking the user about a chat request
void ChatRequest(int serverfd, rio_t rio_serverfd, 
                                        char *name, char *other_user); 
void ChatState(int serverfd, rio_t rio_serverfd, char *name, char *other_user);
void PrintCurrentTime();
void ReadingChatFromServer(void *serverfd_ptr);


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
    char* timeString = NULL;
    char* splitString = NULL;
    char name[MAXLINE];
    
    name[0] = 0; 
    char *envName = getenv("LOGNAME"); // Gets Username from env. variable
    strcat(name, envName);

    // Variables to get local time
    time_t rawtime;
    struct tm * timeinfo;


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
    /////
    
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
    int serverfd = open_clientfd(argv[1], argv[2]);

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



    char server_buf[MAXLINE],command[MAXLINE], 
    arg1[MAXLINE], arg2[MAXLINE];
    
    rio_t rio_serverfd;
    int server_buf_not_empty;


    printf(GREEN "Welcome to the NIXT ChatBox, " RESET);
    printf(CYAN "%s \n" RESET, name);
    printf("Created By Abubaker Omer and Julian Sam \n");
    printf("Type " YELLOW "\"c\"" RESET " to see possible commands\n");
    
    usleep(500);

    while (1) {

        if (feof(stdin)) { /* End of file (ctrl-d) */
            printf ("\nGoodBye!\n");
            exit(0);
        }
        
        /******* reading data from the server *******/

        ioctl(serverfd, SIOCINQ, &server_buf_not_empty);
        if ( server_buf_not_empty )
        {
            read(serverfd, server_buf, MAXLINE);

            sscanf(server_buf,"%s %s %s", command, arg1, arg2);
            
            if (!strcmp(command, "chatans"))
            {
               // Asking the user about a chat request
               ChatRequest(serverfd, rio_serverfd, name, arg1);

            }
            else if (!strcmp(command,"quit"))
            {
                printf("The Server is Shutting Down ...\n" );
                close(serverfd);
            }
            else if (!strcmp(command,"logged"))
            {
                char *logged_in = "This user is already logged in NIXT Chat Server";
                printf("%s \n", logged_in );
                close(serverfd);
                return 0;
            }
        }
        /********************************************/
        printf(">> "); // Type Prompt Symbol 
        fflush(stdout); // Flush to screen
        fgets (buf, MAXLINE, stdin); // Read command line input

        if (!strcmp("c\n",buf))   
            showCommands();

        else if (!strcmp("exit\n",buf) 
                 || !strcmp("quit\n",buf)
                 || !strcmp("q\n",buf) )// Exit client
        {
           
            exit(0);
        }

        else if (!strcmp("chat\n",buf)) 
        {
            startChat(serverfd, rio_serverfd, name);
        }

        else if (!strcmp("group\n",buf)) 
        {
            startGroup();
        }
        else if (!strcmp("users\n",buf) )
        {
            printf("Users online are:\n");
            printUsers(serverfd, buf);
        }
        else if (!strcmp("whoami\n",buf) )
            printf("Name: %s\n", name);
        else 
        {
            time ( &rawtime );
            timeinfo = localtime ( &rawtime );
            splitString  = asctime(timeinfo);

            // Split string to get just the time.
            timeString = strtok(splitString," ");
            timeString = strtok(NULL," ");
            timeString = strtok(NULL," ");
            timeString = strtok(NULL," ");

            printf(YELLOW"[%s] " GREEN "%s: " RESET "%s", timeString, name, buf); 
                                // Print back given input (echo it)
            fflush(stdout); // Flush to screen
        }
    }       
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////
////////////////////////// Server Functions ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void startChat(int serverfd, rio_t rio_serverfd, char *name)
{
    char other_user[MAXLINE], // name of the user the client want to chat with
    chatrqst_instr[MAXLINE], buf[MAXLINE];
    printf("Enter Name/ID: ");
    fflush(stdout);
    fgets (other_user, MAXLINE, stdin); // Read command line input
    
    chatrqst_instr[0] = 0; 
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
    
    JoinChat_instruct[0] = 0; 

    /* Telling your thread in the server-side to be in a ChatState*/
    // join a chat a 1-to-1 chat, joinchat <other user name>
    strcat(strcat(JoinChat_instruct,"joinchat 1to1 "), other_user);
        
    sleep(1);
    
    // Asking the server, to invite the other user to join a chat
    rio_writen(serverfd, JoinChat_instruct, strlen(JoinChat_instruct) + 1);

    ChatState(serverfd, rio_serverfd, name, other_user);

    return;
}



void ChatRequest(int serverfd, rio_t rio_serverfd, char *user, char *other_user)
{   
    char buf[MAXLINE];
    char *other_user_response = calloc(sizeof(char), MAXLINE);
    
    // tell client about the user that want to chat with him/her
    printf("%s wants to chat, do you accept? [Y/N] ", other_user);
    fflush(stdout);

    fgets(buf, MAXLINE, stdin); // Read command line input

    while(strcmp(buf,"n\n") && strcmp(buf,"N\n") 
        && strcmp(buf,"Y\n") && strcmp(buf,"y\n"))
    {
       buf[0] = 0; 
       printf("Please answer with y or n. ");
       fflush(stdout);
       fgets(buf, MAXLINE, stdin); // Read command line input
    }
    
    other_user_response[0] = 0; 

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
        ChatState(serverfd, rio_serverfd, user, other_user);
    }

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
void ChatState(int serverfd, rio_t rio_serverfd, char *client, char *other_user)
{
    char user_text_buf[MAXLINE];
    char meta_info_buf[MAXLINE];

    strcpy(user_text_buf,"");

    printf("Chat Accepted! Joined Chat with " GREEN "%s" RESET "\n", other_user);

    /* Spawning a thread to read messages directly from the server and prints them
       to the client's screen directly as they are recieved */
    pthread_t tid;
    pthread_create(&tid, NULL, (void *)ReadingChatFromServer, (void *)(long *)(&serverfd));
    
    // Reading client' messages until exit command
    while (strcmp("exit\n", user_text_buf))
    { 
        printf(">> ");
        fflush(stdout);
        fgets (user_text_buf, MAXLINE, stdin); // Read command line input
        
        // <my_name> <space> <msg>
        meta_info_buf[0] = 0; 
        strcpy(meta_info_buf,client);
        strcat(meta_info_buf," ");
        strcat(meta_info_buf,user_text_buf);

        rio_writen(serverfd, meta_info_buf, MAXLINE);

        printf(YELLOW"[%s] " GREEN "%s: " RESET "%s\n", "date", client, user_text_buf); 
    }

    // Terminating the reading thread
    pthread_cancel(tid);

    rio_writen(serverfd,"exit",strlen("exit")+1);

    printf("exited chat\n");

    return;
}



// NOTICE: THIS IS INTENDED TO BE AN INFINITE LOOP,
// AND NEED TO BE TERMINARTED BY THE MAIN THREAD
void ReadingChatFromServer(void *serverfd_ptr)
{
    int serverfd = *(int *)(long *)(serverfd_ptr);
    char server_buf[MAXLINE];
    rio_t serverfd_rio;
    rio_readinitb( serverfd_rio, serverfd); 
    int server_buf_not_empty;
    // char * friend_name ;
    while (true)
    { 

        rio_readlineb(serverfd, server_buf, MAXLINE);
        printf("Server sends me: %s", server_buf);
            read(serverfd, server_buf, MAXLINE);
            printf("Server sends me: %s", server_buf);


            // printf(">> ");
            // friend_name = strtok(server_buf," ");
            // if (strcmp(friend_name,myName))
            // {
            //      printf(YELLOW"[%s] " GREEN "%s: " RESET "%s\n", "date", friend_name, server_buf); 
            //      printf("Message recieved from Server: %s\n", server_buf );
            //      printf(">>");
            //      fflush(stdout);
            // }
            // else
            // {
            //     // printf("recieved and not showing (aka success?!?)\n");
            // }

    }

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
    printf(YELLOW "   \"group\"" RESET " : Start a group chat\n");
    printf(YELLOW "   \"exit\" " RESET " : Exit\n");
    return;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////




// Unimplemented Functions
void startGroup()
{
    printf("Group Chat Started?\n");
}

void printHelpInfo()
{
    printf("Chat stuff here bro\n");
}
void PrintCurrentTime()
{


}