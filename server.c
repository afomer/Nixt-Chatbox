/*

 ******************************************************************************
 *                                   server.c                                 *
 *                         The Server of the NIXT-Chatbox                     *
 *                                 Abubaker Omer                              *
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
 *  
 *  UPCOMING...                                                               *
 *
 *  ** Assigning usernames **                                                 *
 *                                                                            *                                       *
 *                                                                            *
 *  ** Sending Msgs **                                                        *
 *                                                                            *                                                                            *
 *  ** Recieving Msgs **                                                      *
 *                                                                            *                                                                            *
 *                                                                            *
 *  ************************************************************************  *

 */

/*
****************************************************************************
---------------------------- Library headers -------------------------------
****************************************************************************

*/

#include "util.h"
#include <stdbool.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>

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

/* The limit for the number of clients allowed */
#define MAX_ONLINE 10

/* The limit for the number of simultaneous chat sessions */
#define MAX_SESSIONS (MAX_ONLINE * MAX_ONLINE)

/*
****************************************************************************
----------------------------------------------------------------------------
****************************************************************************

*/

/*
****************************************************************************
--------------------------- Type definitions -------------------------------
****************************************************************************
*/
typedef struct user *user_t; // client info
typedef struct chat_session *chat_session_t; // info about chat session
typedef struct instruct *instruct_t; // 

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

/* commands inputed by the user */
struct instruct
{
    char* command;
    char* arg1;
    char* arg2;
};

/* info about the client */
struct user
{   
    char *username;
    int client_fd;
    bool chatting;
};

/* info about the chat session */
struct chat_session
{
    char *session_name;
    user_t *users;
    bool group_chat;
};

/*
****************************************************************************
----------------------------------------------------------------------------
****************************************************************************
*/

/*
****************************************************************************
---------------------------- Global Variables ------------------------------
****************************************************************************

*/

/* The Server's Chat sessions */
struct chat_session **chat_session_arr;

/* The Server's Clients */
struct user **online_users_arr;

/* mutex for reading/writing */
sem_t read_write_mutex;

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

void serve_client(void *fd_ptr);
instruct_t read_instruction(char *instruction_buff, size_t buff_size);
void execute_instrcution(char *username, int client_fd, 
                         instruct_t instuc, int *client_exited_addr);

bool is_user_online(char *user);
void start_chat(char *client, char *otherclient);
chat_session_t active_1to1_chat(char *client, char *otherclient);

bool both_in_chat(char *client, char *otherclient, user_t *users);
void join_chat_session(chat_session_t chat, int client_fd);
void send_to_chat_session(chat_session_t chat_session, char *text);

void leave_chat_session(chat_session_t chat_session, int client_fd);
chat_session_t add_chat(char *client, bool group_chat, char *session_name);
int find_client_fd(char *client);

user_t get_user(char *client, int client_fd);
chat_session_t chat_created(char *client, char *otherclient);
chat_session_t one_in_chat(char *otherclient);

void online_user_add(char *user, int client_fd);
void send_to_all_active_clients(char *message);
void online_user_remove(char *user, int client_fd);

bool is_chatting(char *client, int client_fd);
bool group_exist(char *group_name);
chat_session_t get_chat(char *chat_session_name, bool group_chat);
char *list_users(chat_session_t chat_session);

/*
****************************************************************************
----------------------------------------------------------------------------
****************************************************************************
*/

/*
*********************************************
*********************************************
******** The Server's Main Function *********
*********************************************
*********************************************
*
* main: Initialize the different global variables,
* open a listening socket, and for  every connection 
* recieved a corresponding detached thread is spawned,
* the thread's function is serve_client().
*
*/
int main(int argc, char **argv) 
{   
   /* initliazing variables essential for client-server connection */
    int listenfd, *connfd;

    socklen_t clientlen;

    struct sockaddr *clientaddr = NULL;

    /* Retrieving the port from the command line args */
    if (argc != 2)
    {
      fprintf(stderr, "usage: %s <port>\n", argv[0]);
      exit(1);
    }
    
    /* Setting a socket to listen to clients' connection requests */
    
    clientlen = sizeof(struct sockaddr);
       
    listenfd = open_listenfd(argv[1]);

    if (listenfd < 1)
    {
      printf("Failed to Open a socket for listening\n");
      exit(1);
    }

    /* Allocating arrays for online users and chat sessions */
    
    chat_session_arr = calloc(sizeof(struct chat_session), MAX_SESSIONS);

    online_users_arr = calloc(sizeof(struct user), MAX_ONLINE);
    
    /* Set the mutex for reading/writing to be 1 initially */
    sem_init(&read_write_mutex, 0, 1);


    /* Listen to clients' connection requests and spawn a "serving" thread for each client */
    while (true)
    {
    
      pthread_t tid;

      clientlen = sizeof(clientaddr);
    
      connfd = malloc(sizeof(int));

      *connfd = accept(listenfd, clientaddr, &clientlen);
    
      pthread_create(&tid, NULL, (void *)serve_client, (void *)((long *)connfd));
    
    }
    

    // Should never reach here
    /* before exiting the server, free allocated arrays */
    free(chat_session_arr);
    free(online_users_arr);
    send_to_all_active_clients("quit");
 
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * serve_client: Communicating with the client-code      
 * and executing instructions accordingly in a read-execute model.
 * The first datum sent by the user should be her/his name as an
 * authentication handshake. If the username is already provided for an active
 * user the function will exit. After that, in the case of successful handshake,
 * the function wait for the client's commands
 * and execute them as they are recieved.
 */
void serve_client(void* fd_ptr)
{    
    /* Making the thread detached */
    pthread_detach(pthread_self());
    
    /* retrieving the fd of the client */
    int client_fd = *((int *)(long *)(fd_ptr));
    free(fd_ptr);

    /* initializing different buffers */
    char buf[MAXLINE], 
    instruction_buff[MAXLINE],
    username[MAXLINE];

    /* The first data sent from the client is his/her name
       if the client-code failed to provide a useranme
       close the connection with the client */
    if (!read(client_fd, buf, MAXLINE)) 
    {   
        /* failure of identification */
        close(client_fd);
        return;
    }
    
    /* storing the username in the username buffer */
    sscanf(buf, "%s", username);
    
    // Client's name is printed for debugging purposes
    printf("Username: %s\n", username );      

    /* if the user isn't in the array of online users, add him/her
       otherwise close the connection */
    if (!is_user_online(username))
    {  
        /* add the user to the online users array, if there is space */
        online_user_add(username, client_fd);
    }
    else
    {   
        /* Other active user is already logged with the same name */
        char *logged_in = "logged\n";
        rio_writen(client_fd, logged_in,strlen(logged_in)+1);
        close(client_fd);
        return;
    }

    /* variables to keep track of the number of bytes read from the user */
    int bytes_read;
    int client_exited = 0;
    int *client_exited_addr = &client_exited;
    
    /* Keep reading the user instructions, and execute them accordingly
       until the send an exit command */
    while (!client_exited)
    {

        bytes_read = read(client_fd, instruction_buff, MAXLINE);
        
        /* in case of read error, exit the loop */
        if (bytes_read < 0)
          break;
        

        instruct_t instruct = read_instruction(instruction_buff, bytes_read);

        execute_instrcution(username, client_fd, instruct, client_exited_addr);
        
    }
    
    close(client_fd);
    return;

}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * execute_instrcution: execute the instruction given in 
 * the instruction struct with the arguments given, if the command is valid.
 * otherwise, ignore the command.               
 * 
 */
void execute_instrcution(char *client, int client_fd, instruct_t instruct, int *client_exited_addr)
{ 
   /* if the instruction is NULL, the command is invalid. Ignore it */
   if ( instruct == NULL )
    {   
        return;
    }
   
    /* Make a local copy of the command and the 
      arguments inputed with it and execute them */

    char *command = instruct->command;
    char *arg1 = instruct->arg1;
    char *arg2 = instruct->arg2;
        

    // client quitting Nixt's Chatbox
    if (!strcmp(command, "exit"))
    {   
        // remove the client from the array of active users
        // and set the exit variable to 1 to close the connection 
        // with that client
        online_user_remove(client, client_fd);
        *client_exited_addr = 1;
        return;
    }
    else if (!strcmp(command, "chatrqst")) // Chat invite to the user in <arg1>
    {
        
        // If the other client is not online, tell the client
        // If the other client in an active chat, tell the client
        char chat_request[MAXLINE];
        chat_request[0] = 0;

        if (!is_user_online(arg1))
        {
          rio_writen(client_fd,"offline\n", strlen("offline\n") + 1);
          return;
        }
        else if (is_chatting(arg1, -1))
        {
          rio_writen(client_fd,"chatting\n", strlen("chatting\n") + 1);
          return;
        }
        
        // Otherwise invite the other client to chat, and depending
        // on his/her response join a chat or don't.
        char chat_request_answer[MAXLINE];
        chat_request_answer[0] = 0;
       
        int otherclient_fd = find_client_fd(arg1);

        strcat(strcat(chat_request, "chatans "), client); // chatans <me>
        
        rio_writen(otherclient_fd, chat_request, strlen(chat_request) + 1);

        size_t buff_size = read(client_fd, chat_request_answer,MAXLINE);
        
        // read the response, then parse it and execute it with a recursive call
        execute_instrcution(client, client_fd, 
            read_instruction(chat_request_answer,buff_size), client_exited_addr);

    }
    else if (!strcmp(command, "crtgrp")) // Creating group with the name <arg1>
    {   
        // after creating the group, tell the user the client, the creation is done
        add_chat(client, true, arg1);
        rio_writen(client_fd, "created", strlen("created")+1);
    }
    else if (!strcmp(command, "chatansno")) // Rejecting a chat invite with <arg1>
    {  
       // telling the otherclient, that the client he invited, 
       // rejected the chat invitation       
       
       char chat_request_answer[MAXLINE];
       chat_request_answer[0] = 0;
       
       int otherclient_fd = find_client_fd(arg1);
       
       // <rejected> <person who rejected (otherclient)>

       strcat(strcat(chat_request_answer, "rejected "), client);

       rio_writen(otherclient_fd, chat_request_answer, 
                               strlen(chat_request_answer)+1);

    }
    else if (!strcmp(command, "chatansyes")) // Accepting a chat invite with <arg1>
    {  
        // telling the otherclient, that the client he invited, 
        // accepted the chat invitation
        char chat_request_answer[MAXLINE];
        chat_request_answer[0] = 0;
       
        int otherclient_fd = find_client_fd(arg1);
       
        // <accepted> <person who accepted (otherclient)>

        strcat(strcat(chat_request_answer, "accepted "), client);
        
        // make a chat session, join it, and go into a "chat state"
        chat_session_t new_chat = add_chat(client, false, client);
       
        rio_writen(otherclient_fd, chat_request_answer, 
                               strlen(chat_request_answer) + 1);       

        join_chat_session( new_chat, client_fd);
       

       
    }
    else if (!strcmp(command, "joinchat") && !strcmp(arg1, "1to1")) 
    {  
      // Joining a 1 to 1 chat with <arg2>
      // join the chat session that only have 1 user which is the otherclient
      join_chat_session( one_in_chat(arg2), client_fd);

    }
    else if (!strcmp(command, "joinchat") && !strcmp(arg1, "grp"))
    {  
      // Joining a group chat titled <arg2>
      join_chat_session( get_chat(arg2, true), client_fd);

    }
    else if (!strcmp(command, "users")) // The list of active clients 
    {   
        char users[MAXLINE];
        users[0] = 0;
        
        // Concat the names of online users and send them to the client

        int i;
        for (i = 0; i < MAX_ONLINE; i++)
        {
            sem_wait(&read_write_mutex);
            if ( online_users_arr[i] != NULL )
            { 
              strcat(strcat(users, online_users_arr[i]->username), " \n");
              printf("%s\n", online_users_arr[i]->username);
            }
            sem_post(&read_write_mutex);
        }

        rio_writen(client_fd, users, strlen(users) + 1);

    }
    else if (!strcmp(command, "grpexist")) // Group existence in the Server
    {   
      // If the group <arg1> exists send yes otherwise send no
      char users[MAXLINE];
      users[0] = 0;
      chat_session_t tmp_chat;

      if ( (tmp_chat = get_chat(arg1, true)) != NULL )
      {
          rio_writen(client_fd, "yes", strlen("yes") + 1);
      }
      else
      {
          rio_writen(client_fd, "no", strlen("no") + 1);
      }
    }
    else if (!strcmp(command, "groups") || !strcmp(command, "grps")) 
    {   
      // The List of created groups
      
      // Concat the names of active group chats and send them to the client

      char users[MAXLINE];
      users[0] = 0;
      int i;

      for (i = 0; i < MAX_SESSIONS; i++)
        {
          sem_wait(&read_write_mutex);
          if ( chat_session_arr[i] != NULL
               && chat_session_arr[i]->group_chat)
          { 
            strcat(strcat(users, chat_session_arr[i]->session_name), " \n");
          }
          sem_post(&read_write_mutex);
        }

        rio_writen(client_fd, users, strlen(users) + 1);

    }
    else if (!strcmp(command, "list") || !strcmp(command, "list\n"))
    {  
      // List users in the chat session titled <arg1>
      // Concat the names of active clients in the chat session named <arg1>
      // and send them to the client
      // (group and 1 to 1 chats)

      char users[MAXLINE];
      users[0] = 0;
      int i;

      chat_session_t tmp_chat = get_chat(arg1, false);

      if ( tmp_chat == NULL )
      {
          rio_writen(client_fd, "", strlen("") + 1);
      }
      else
      {

          for (i = 0; i < MAX_ONLINE; i++)
          {   
              sem_wait(&read_write_mutex);
              if ( tmp_chat->users[i] != NULL)
              { 
                strcat(strcat(users, tmp_chat->users[i]->username), " \n");
              }
              sem_post(&read_write_mutex);
          }

          rio_writen(client_fd, users, strlen(users) + 1);
      }
    }

    // Freeing the instruction struct before exiting the function

    free(instruct->command);
    free(instruct->arg1);
    free(instruct->arg2);
    free(instruct); 
    
    return;

}
/*
----------------------------------------------------------------------------
*/

/* 
 * read_instruction: return the command
 * name and arguemnt with it in a struct (instruct_t),in case of success,
 * otherwise return NULL
 */
instruct_t read_instruction(char *instruction_buff, size_t buff_size)
{   
    char *command = malloc(MAXLINE);
    char *arg1 = malloc(MAXLINE);
    char *arg2 = malloc(MAXLINE);
    
    // For debugging purposes, print the instruction sent
    printf("instruction_buff: %s\n", instruction_buff );

    /* if no command is found just free the allocated blocks and return NULL */
    if ( sscanf(instruction_buff, "%s %s %s", command, arg1, arg2) < 1
       && sscanf(instruction_buff, "%s %s %s\n", command, arg1, arg2) < 1 )
    {

        free(arg2);

        free(arg1);
        
        free(command);

        return NULL;
    }
      
    /* Make an instruction struct and put in it the command and the args */
    instruct_t instruction = malloc(sizeof(struct instruct));

    instruction->command = command;
    instruction->arg1 = arg1;
    instruction->arg2 = arg2;


    return instruction;
}
/*
----------------------------------------------------------------------------
*/

/* 
 * online_user_add: Adding users to the online users array
 */
void online_user_add(char *user, int client_fd)
{
    int i;

    /* look for an empty spot in the online users array,
       if no empty slot is found, tell the client and close the connection */
    for (i = 0; i < MAX_ONLINE; i++)
    {
                
        // If an empty array slot is found update it fields
        sem_wait(&read_write_mutex);
        if ( online_users_arr[i] == NULL)
        {   
    
          online_users_arr[i] = malloc(sizeof(struct user));

          online_users_arr[i]->username = user;

          online_users_arr[i]->client_fd = client_fd;

          online_users_arr[i]->chatting = false;
          
          sem_post(&read_write_mutex);
          
          return;       
        }

        sem_post(&read_write_mutex);
        

    }
    
    // Should not reach here
    rio_writen(client_fd, "full",strlen("full") + 1);

    close(client_fd);     
    
    exit(0);
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * is_chatting: return true if and only if the client is chatting
 *
 */
bool is_chatting(char *client, int client_fd)
{
    int i;

    /* look for the client in the online users array using his/her fd and username,
       if the client is not found, tell the client to quit, and close the connection */
    for (i = 0; i < MAX_ONLINE; i++)
    {
                
        // If an empty cache slot is found update it fields
        sem_wait(&read_write_mutex);
        if ( online_users_arr[i] != NULL 
           && (online_users_arr[i]->client_fd == client_fd
            || !strcmp(client, online_users_arr[i]->username) ) )
        {   
    
            if (online_users_arr[i]->chatting)
            {
                sem_post(&read_write_mutex);     
                return true;
            }
            else
            {
                sem_post(&read_write_mutex);     
                return false;
            }
          
        }

        sem_post(&read_write_mutex);
        

    }
    
    // Should not reach here
    rio_writen(client_fd, "quit",strlen("quit") + 1);

    close(client_fd);     
    
    exit(0);
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * online_user_remove: Removing users from the online users array
 *
 */
void online_user_remove(char *user, int client_fd)
{
    int i;

    /* look for the client in the online users array using his/her fd and username
        and replace his/her position with NULL */
    for (i = 0; i < MAX_ONLINE; i++)
    {
                
        // If an empty cache slot is found update it fields
        sem_wait(&read_write_mutex);
        if ( online_users_arr[i] != NULL
             && (online_users_arr[i]->client_fd == client_fd 
                      || !strcmp(user, online_users_arr[i]->username) ) )
        {   
    
          online_users_arr[i] = NULL;
          
          sem_post(&read_write_mutex);
          
          return;       
        }

        sem_post(&read_write_mutex);
        

    }
        
    
    return;
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * is_user_online: Checking if the user is in
 * the online users array
 *
 */
bool is_user_online(char *user)
{  
    int i;

    for (i = 0; i < MAX_ONLINE; i++)
    {   
        sem_wait(&read_write_mutex);
        if ( online_users_arr[i] != NULL && !strcmp(user, online_users_arr[i]->username))
        {
          sem_post(&read_write_mutex);
          return true;
        }
        sem_post(&read_write_mutex);
    }

    return false;
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * start_chat: Starting a chat session between 
 * two clients in the online users array
 *
 */
void start_chat(char *client, char *otherclient)
{   
    // check if there is an active chat session, if there is no
    // active chat session, make one, then join it
    chat_session_t chat;
    
    // if the there is an active chat it will be returned
    if ( (chat = active_1to1_chat(client, otherclient)) == NULL )
    {   
        // making a chat session include joining it
        if ( (chat = chat_created(client,otherclient)) == NULL )
            chat = add_chat(client, false, client);
    }
    
    /* joinning a chat session mean you will start to recieve and send msgs */
    /* (Chat State)*/
    join_chat_session(chat, find_client_fd(client));

    return;
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * join_chat_session: Add the client to the array of active users in the chat session and 
 * start sending and receiving messages in the chat session 
 *
 */
// REQUIRES: valid(client_fd) == true
void join_chat_session(chat_session_t chat_session, int client_fd)
{
    /* Add the client to the chat_session users,
     keep looping until the user exits. While looping, receive
     client's messages, and send them to all users in the chat_session */
    
    // buffer for reading user messages
    char buf[MAXLINE];
    buf[0] = 0;
    
    // retrieving the users' info 
    user_t user = get_user(NULL, client_fd);
    
    // Setting user to be in a chatting state
    sem_wait(&read_write_mutex);
    user->chatting = true;
    sem_post(&read_write_mutex);

   
   // Adding the client, to the chat-session-users array
   int i;
   for (i = 0; i < MAX_ONLINE; i++)
   {

      sem_wait(&read_write_mutex);
      if ( chat_session->users[i] == NULL)
       {
         chat_session->users[i] = malloc(sizeof(struct user));
         chat_session->users[i]->username = malloc(strlen(user->username) + 1);
         strcpy(chat_session->users[i]->username, user->username);
         chat_session->users[i]->client_fd = client_fd;
         chat_session->users[i]->chatting = true;
         sem_post(&read_write_mutex);
         break; // sorry Prof. Saquib
       }

      sem_post(&read_write_mutex);
   }
    
    /* Be in a ChatState */

    // For debugging purposes:
    // the name of the client who is joining the chat is printed
    printf("Server-Side: %s joinning chat \n", user->username );

    int client_buf_not_empty;

    /* Send all msgs the user type to all people in the chat session
    until the user exits */ 
    while ( strcmp(buf ,"exit\n") && strcmp(buf ,"exit") )
    {   
      
        /// Recieving Other participants Chat messages, if there is.
        ioctl(client_fd, SIOCINQ, &client_buf_not_empty);
        
        if ( client_buf_not_empty )
        {
            read(client_fd, buf, MAXLINE);
          
            // list command, to show the client current active users in the chat
            if ( !strcmp("list\n", buf) || !strcmp("users\n", buf)) // list <chat_session_name>
            {
              strcpy(buf, list_users(chat_session));

              rio_writen(client_fd, buf, strlen(buf) + 1);
            }
            else if ( strcmp(buf ,"exit\n") && strcmp(buf ,"exit"))
            {
              send_to_chat_session(chat_session, buf);
            }
            // For debugging purposes:
            // Printing the chat messages
            printf("Chat >> %s\n", buf );

        }
        

    }

    // For debugging purposes:
    // Printing the name of the user that exited the chat
    printf("%s left chat\n", user->username );

    // Leaving the chat session
    leave_chat_session(chat_session, client_fd);

}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * leave_chat_session: Leave a chatting session  
 *
 */
void leave_chat_session(chat_session_t chat_session, int client_fd)
{
  int i;

  for (i = 0; i < MAX_ONLINE; i++)
  {   
      sem_wait(&read_write_mutex);

      if ( chat_session->users[i] != NULL 
           && chat_session->users[i]->client_fd == client_fd )
      {
          user_t tmp_user = get_user(chat_session->users[i]->username, client_fd);
          tmp_user->chatting = false;
          chat_session->users[i] = NULL;
      }

      sem_post(&read_write_mutex);
  }
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * send_to_chat_session: Send a message to every user in
 * the chat session    
 *
 */
void send_to_chat_session(chat_session_t chat_session, char *text)
{   
    int i;
    
    int user_fd, text_len_p1 = strlen(text) + 1; 
    
    // Fod debugging purposes, the text sent is printed in the server-side
    printf("SEND: %s\n", text );

    for ( i = 0; i < MAX_ONLINE; i++)
      { 
        sem_wait(&read_write_mutex);
        if ( chat_session->users[i] != NULL )
        {
          user_fd = chat_session->users[i]->client_fd;
          printf("%d, ", user_fd );              
          write(user_fd, text, text_len_p1);
        }
        sem_post(&read_write_mutex);
      }

    printf("\n");
    
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * active_1to1_chat: return the chat session which have                  
 * the two clients only. Return NULL if it's not found 
 *
 */
chat_session_t active_1to1_chat(char *client, char *otherclient)
{  
   chat_session_t tmp_chat;
   user_t *chat_users;

   int i;
   
   for (i = 0; i < MAX_SESSIONS; i++)
   {    
        sem_wait(&read_write_mutex);
        tmp_chat = chat_session_arr[i];
        chat_users = tmp_chat->users;
        sem_post(&read_write_mutex);

       if ( tmp_chat != NULL
         && both_in_chat(client, otherclient, chat_users))
        {
          sem_wait(&read_write_mutex);

          if ( tmp_chat->group_chat == false)
          {
            sem_post(&read_write_mutex);
            return tmp_chat;
          }
          sem_post(&read_write_mutex);
        }
   }

   return NULL;
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * chat_created: return a chat session, if there is a chat 
 * with only 1 user, that is the user him/herself. Otherwise, return NULL
 *
 */
chat_session_t chat_created(char *client, char *otherclient)
{  
   chat_session_t tmp_chat;

   int i;
   
   for (i = 0; i < MAX_SESSIONS; i++)
   {   
        sem_wait(&read_write_mutex);
        tmp_chat = chat_session_arr[i];
        sem_post(&read_write_mutex);
        
        sem_wait(&read_write_mutex);
        if ( tmp_chat != NULL
         && tmp_chat->group_chat == false)
        {
          sem_post(&read_write_mutex);
          return tmp_chat;
        }
        sem_post(&read_write_mutex);
   }

   return NULL;
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * add_chat: make a chat session, group or 1to1, title it with the session_name
 * and add the chat to the active chat sessions array 
 *
 */
chat_session_t add_chat(char *client, bool group_chat, char *session_name)
{
   /* when you add a chat session, you join it after that. 
    Since this happen before sending the acceptance of 
    the chat invitation, it's expected that the other user find 
    and join this session after he/she receives the acceptance */

   chat_session_t tmp_chat;

   int i;
   
   for (i = 0; i < MAX_SESSIONS; i++)
   {
      
      sem_wait(&read_write_mutex);
      tmp_chat = chat_session_arr[i];
      sem_post(&read_write_mutex);

      if ( tmp_chat == NULL)
       {
            sem_wait(&read_write_mutex);
            
            chat_session_arr[i] = malloc(sizeof(struct chat_session));
            chat_session_arr[i]->group_chat = group_chat;
            chat_session_arr[i]->users = calloc(sizeof(struct user), MAX_ONLINE);
            chat_session_arr[i]->session_name = malloc(strlen(client)+1);
            strcpy(chat_session_arr[i]->session_name, session_name);
            tmp_chat = chat_session_arr[i];
            
            sem_post(&read_write_mutex);

            return tmp_chat;
       } 
   }
   
   // if reached here there is an error
   return NULL;
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * both_in_chat: return true if both users are in 
 * a one-to-one active chat.
 *
 */

bool both_in_chat(char *client, char *otherclient, user_t *users)
{  
   if (users == NULL)
    return false;
   
   bool client_available = false;
   bool otherclient_available = false;
   char *tmp_username;
   int i;

   for (i = 0; i < MAX_ONLINE; i++)
   {
      if ( users[i] != NULL )
       {
          sem_wait(&read_write_mutex);
          tmp_username = users[i]->username;
          sem_post(&read_write_mutex);

          if (!strcmp(client, tmp_username))
           client_available = true;

          if (!strcmp(otherclient, tmp_username))
           otherclient_available = true;

       }
   
   }

   if ( client_available && otherclient_available )
      return true;
   else
      return false;
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * one_in_chat: return a chat session that only have the otherclient in it
 * (the person that the client want to chat with).
 * If not found return NULL. 
 *
 */

chat_session_t one_in_chat(char *otherclient)
{  

    if (otherclient == NULL)
    return NULL;
   
    int i;
    chat_session_t tmp_chat;

    for (i = 0; i < MAX_SESSIONS; i++)
    {  
        sem_wait(&read_write_mutex);
        tmp_chat = chat_session_arr[i];
        sem_post(&read_write_mutex);

        sem_wait(&read_write_mutex);
        if ( tmp_chat != NULL 
          && !strcmp(otherclient, tmp_chat->session_name)
          && tmp_chat->group_chat == false )
        {

          sem_post(&read_write_mutex);
          return tmp_chat;

        }
        sem_post(&read_write_mutex);
   }

  
  return NULL;

}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * find_client_fd: return the file descriptor of the corresponding client
 * with the given username
 *
 */
int find_client_fd(char *client_username)
{
   int i, client_fd;

   for (i = 0; i < MAX_ONLINE; i++)
   {   
       sem_wait(&read_write_mutex);
       user_t tmp_user = online_users_arr[i];
       sem_post(&read_write_mutex);

       sem_wait(&read_write_mutex);
       if (tmp_user != NULL 
          && !strcmp(client_username, tmp_user->username))
        {
            client_fd = tmp_user->client_fd;
            sem_post(&read_write_mutex);

            return client_fd;
        }

        sem_post(&read_write_mutex);

   }

   return -1;
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * send_to_all_active_clients: send a message to all active users
 * in the online user array
 *
 */
void send_to_all_active_clients(char *message)
{
  int i;
   for (i = 0; i < MAX_ONLINE; i++)
   {   
        sem_wait(&read_write_mutex);
        user_t tmp_user = online_users_arr[i];
        sem_post(&read_write_mutex);

        sem_wait(&read_write_mutex);
        if (tmp_user != NULL)
            rio_writen(tmp_user->client_fd, message, strlen(message)+1);
        
        sem_post(&read_write_mutex);
   }

   return;
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * want_to_chat: return true if the other client
 * want to chat, otherwise return false
 *
 */
bool want_to_chat(char *client, char *otherclient)
{
    int otherclient_fd = find_client_fd(otherclient);
    
    char buf[MAXLINE];
    

    /* first send a "ask connect", then the name of the client
       so the client-side identfiy the command first */
    rio_writen(otherclient_fd, "askcnct", strlen("askcnct")+1);

    rio_writen(otherclient_fd, client, strlen(client)+1);

    read(otherclient_fd, buf, MAXLINE);

    if ( !strcmp(buf,"n") || !strcmp(buf,"N"))
        return false;

    else 
        return true; 
}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * group_exist: return true iff there exist a chat session 
 * with the given name 
 *
 */
bool group_exist(char *group_name)
{   
    int i;
    chat_session_t tmp_chat;
    for (i = 0; i < MAX_SESSIONS; i++)
     {    
        sem_wait(&read_write_mutex);
        tmp_chat = chat_session_arr[i];
        sem_post(&read_write_mutex);
        
        sem_wait(&read_write_mutex);

        if ( tmp_chat != NULL
          && tmp_chat->group_chat 
          && !strcmp(tmp_chat->session_name, group_name))
        {

          if ( tmp_chat->group_chat == false)
          {
              sem_post(&read_write_mutex);
              return true;
          }

          sem_post(&read_write_mutex);
        }
     }

    return false;

}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * get_chat: return chat session struct for a chat session with the given name               
 * 
 */
chat_session_t get_chat(char *chat_session_name, bool group_chat)
{  
  int i;
   
  for (i = 0; i < MAX_SESSIONS; i++)
   {   
       sem_wait(&read_write_mutex);
       chat_session_t tmp_chat = chat_session_arr[i];
       sem_post(&read_write_mutex);

       sem_wait(&read_write_mutex);
       if (tmp_chat != NULL 
        && ( !group_chat || tmp_chat->group_chat )
        && !strcmp(chat_session_name, tmp_chat->session_name))
        {
          sem_post(&read_write_mutex);
          return tmp_chat;
        }

       sem_post(&read_write_mutex);
   }

   return NULL;

}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * get_user: return user struct for a client with the given username               
 * and/or file descriptor                           
 * 
 */
user_t get_user(char *user, int client_fd)
{  
  int i;
   
  for (i = 0; i < MAX_ONLINE; i++)
   {   
       sem_wait(&read_write_mutex);
       user_t tmp_user = online_users_arr[i];
       sem_post(&read_write_mutex);

       sem_wait(&read_write_mutex);

       if (tmp_user != NULL && ( ( user != NULL && !strcmp(user, tmp_user->username))
                                   || tmp_user->client_fd == client_fd) )
        {
          sem_post(&read_write_mutex);
          return tmp_user;
        }

       sem_post(&read_write_mutex);
   }

   return NULL;

}
/*
----------------------------------------------------------------------------
*/

/*
 * 
 * list_users: return a string of active clients currently in the chat session given.                         
 * 
 */
char *list_users(chat_session_t chat_session)
{   
    char users[MAXLINE];
    users[0] = 0;
    int i;

    user_t *chat_users = chat_session->users;

    if ( chat_users == NULL )
    {
          return NULL;
    }
    else
    {

          for (i = 0; i < MAX_ONLINE; i++)
          {   
              sem_wait(&read_write_mutex);
              if ( chat_users[i] != NULL)
              { 
                strcat(strcat(users, chat_users[i]->username), " \n");
              }
              sem_post(&read_write_mutex);
          }

    }
    
    char *tmp_user = users;
   
    return tmp_user;
}
/*
----------------------------------------------------------------------------
*/
