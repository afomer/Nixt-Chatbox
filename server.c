#include "util.h"
#include <stdbool.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
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
 *   the array of online users. After that commands are waited from the client* 
 *   to either send, recieve msgs or join group chats.                        *
 *  
 *  UPCOMING...                                                              *
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

/* struct for commands inputed by the user */
struct instruct
{
    char* command;
    char* arg1;
    char* arg2;
};


struct user
{   
    char *username;
    int client_fd;
    bool chatting;
};

typedef struct user *user_t;

struct chat_session
{
    char *session_name;
    user_t *users;
    bool group_chat;
};

typedef struct chat_session *chat_session_t; 

typedef struct instruct *instruct_t;

#define MAX_ONLINE 10
#define MAX_SESSIONS (MAX_ONLINE * MAX_ONLINE)

// The Array of all sessions
struct chat_session **chat_session_arr;

// The Array of Online Users
struct user **online_users_arr;

/* mutex for reading/writing */
sem_t read_write_mutex;

/* Client functions */

void serve_client(void *);
instruct_t read_instruction(char *instruction_buff, size_t buff_size);
void execute_instrcution(char *username, int client_fd, 
              rio_t rio_client_fd , instruct_t instuc, int *client_exited_addr);
bool is_user_online(char *user);
void start_chat(char *client, char *otherclient, rio_t rio_client_fd);
chat_session_t active_1to1_chat(char *client, char *otherclient);
bool both_in_chat(char *client, char *otherclient, user_t *users);
void join_chat_session(chat_session_t chat, int client_fd, rio_t rio_client_fd);
void send_to_chat_session(chat_session_t chat_session, char *text);
void leave_chat_session(chat_session_t chat_session, int client_fd);
chat_session_t add_chat(char *client, bool group_chat, char *session_name);
int find_client_fd(char *client);
bool want_to_chat(char* client, char *otherclient);
user_t get_user(char *client, int client_fd);
chat_session_t chat_created(char *client, char *otherclient);
chat_session_t one_in_chat(char *otherclient);
void online_user_add(char *user, int client_fd);
void send_to_all_active_clients(char *message);
bool empty_chat_session(chat_session_t chat_session);
void online_user_remove(char *user, int client_fd);
bool is_chatting(char *client, int client_fd);
bool group_exist(char *group_name);
chat_session_t get_chat(char *chat_session_name, bool group_chat);
char *list_users(chat_session_t chat_session);
/*
 * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * *
 * The Central Server Main Function  *
 * * * * * * * * * * * * * * * * * * *
 * * * * * * * * * * * * * * * * * * *
 */

int main(int argc, char **argv) 
{   
   /* initliazing file descriptors for the client and the server */
    int listenfd, *connfd;

    socklen_t clientlen;

    struct sockaddr *clientaddr = NULL;

    /* Retrieving the port from the command line args */
    if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
    }
    
    clientlen = sizeof(struct sockaddr);
       
    /* Setting a socket to listen to clients' connection requests */
    listenfd = open_listenfd(argv[1]);

    if (listenfd < 1)
    {
      printf("socket failure\n");
      exit(1);
    }

    /* Allocating arrays for online users and active chat sessions */
    
    chat_session_arr = calloc(sizeof(struct chat_session), MAX_SESSIONS);

    online_users_arr = calloc(sizeof(struct user), MAX_ONLINE);

    sem_init(&read_write_mutex, 0, 1);


    /* Listen to client connection requests and spawn threads for each client */
    while (true)
    {
    
      pthread_t tid;

      clientlen = sizeof(clientaddr);
    
      connfd = malloc(sizeof(int));

      *connfd = accept(listenfd, clientaddr, &clientlen);
    
      pthread_create(&tid, NULL, (void *)serve_client, (void *)((long *)connfd));
    
    }
    


    /* before exiting the server, free allocated arrays */
    free(chat_session_arr);
    free(online_users_arr);

    send_to_all_active_clients("quit");
 
}

/*
 * * * * * * * * * * * * * * * * * * * * * *
 * serve_client:                           *
 * Communicating with the client-code      *
 * and executing instructions accordingly  *
 * * * * * * * * * * * * * * * * * * * * * *
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

    rio_t rio_client_fd; 
    
    rio_readinitb(&rio_client_fd, client_fd); 

    /* the first data sent from the client is his/her name
       if the client-code failed to provide a useranme
       close the connection with the client */
    if (!rio_readlineb(&rio_client_fd, buf, MAXLINE)) 
    {   
        /* failure of identification */
        close(client_fd);
        return;
    }
    
    /* storing the username in the username buffer */
    sscanf(buf, "%s", username);

    printf("username: %s\n", username );      

    /* if the user isn't in the array of online users, add him/her
       otherwise close the connection */
    if (!is_user_online(username))
    {  
        /* add the user to the online users array, if there is space */
        online_user_add(username, client_fd);
    }
    else
    {   
        /* the user is already logged in another thread */
        char *logged_in = "logged\n";
        rio_writen(client_fd, logged_in,strlen(logged_in)+1);
        close(client_fd);
        return;
    }

    /* variables to keep track of the number of bytes read from the user */
    int bytes_read;

    int client_exited = 0;
    int *client_exited_addr = &client_exited;
    
    /* Keep reading the user instructions, and execute them accordingly */
    while (!client_exited)
    {

        bytes_read = 0;
        
        bytes_read = read(client_fd, instruction_buff, MAXLINE);

        if (bytes_read < 0)
          break;
        
        // Checking broken pipe
        if (errno == EPIPE)
        {
            printf("Lost Connection with %s\n", username);
            break;
        }

        instruct_t instruct = read_instruction(instruction_buff, bytes_read);

        execute_instrcution(username, client_fd, rio_client_fd, instruct, client_exited_addr);
        
    }
    
    //Never reache here
    close(client_fd);
    return;

}

/*
 * * * * * * * * * * * * * * * * * * * * * *
 * execute_instrcution:                    *
 * execute the instruction with the        *
 * arguments in the                        *
 * instruction struct given                *
 * * * * * * * * * * * * * * * * * * * * * *
 */
void execute_instrcution(char *client, int client_fd, 
            rio_t rio_client_fd , instruct_t instruct, int *client_exited_addr)
{ 
   /* if the instruction is NULL tell the user the command is invalid */
   if ( instruct == NULL )
    {   
        return;
    }
   
    /* Otherwise make a local copy of the command and the arguments
       and examine them */

    char *command = instruct->command;
    char *arg1 = instruct->arg1;
    char *arg2 = instruct->arg2;
    
    //printf("command: %s, %s %s \n", command, arg1, arg2 );
    
   // printf("%s \n", command );
    // exit instruction, to exit the chat client
    if (!strcmp(command, "exit"))
    {   
        online_user_remove(client, client_fd);
        printf("\n");
        *client_exited_addr = 1;
        return;
    }
    else if (!strcmp(command, "chatrqst")) // chatrqst <otherclient>
    {
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

        // Ask the client if he want to chat with the otherclient       
        char chat_request_answer[MAXLINE];
        chat_request_answer[0] = 0;
       
        int otherclient_fd = find_client_fd(arg1);

        strcat(strcat(chat_request, "chatans "), client); // chatans <me>
        rio_writen(otherclient_fd, chat_request, strlen(chat_request) + 1);

        size_t buff_size = read(client_fd, chat_request_answer,MAXLINE);
        
        // read the response, then parse it and execute it with a recursive call
        execute_instrcution(client, client_fd, rio_client_fd, 
            read_instruction(chat_request_answer,buff_size), client_exited_addr);

    }
    else if (!strcmp(command, "crtgrp")) // chatrqst <otherclient>
    {
        add_chat(client, true, arg1);
        rio_writen(client_fd, "created", strlen("created")+1);
    }
    else if (!strcmp(command, "chatansno")) // chatansno <otherclient>
    {  
       // telling the otherclient, that the user rejected the request       
       char chat_request_answer[MAXLINE];
       chat_request_answer[0] = 0;
       
       int otherclient_fd = find_client_fd(arg1);
       
       // <rejected> <person who rejected (otherclient)>

       strcat(strcat(chat_request_answer, "rejected "), client);

       rio_writen(otherclient_fd, chat_request_answer, 
                               strlen(chat_request_answer)+1);

    }
    else if (!strcmp(command, "chatansyes")) // chatansno <otherclient>
    {  
       // telling the otherclient, that the user accepted the request       
       char chat_request_answer[MAXLINE];
       chat_request_answer[0] = 0;
       
       int otherclient_fd = find_client_fd(arg1);
       
       // <rejected> <person who rejected (otherclient)>

       strcat(strcat(chat_request_answer, "accepted "), client);


       // make a chat, join it, and be in a chat state 
       // (in chat state don't send until there is at least 1 user)
       printf("%s joining with %s\n",client, arg1);

       chat_session_t new_chat = add_chat(client, false, client);
       
       rio_writen(otherclient_fd, chat_request_answer, 
                               strlen(chat_request_answer) + 1);       

       join_chat_session( new_chat, client_fd, rio_client_fd );
       

       
    }
    else if (!strcmp(command, "joinchat") && !strcmp(arg1, "1to1")) // joinchat 1to1 <otherclient>
    {  
      // join the chat session that only have 1 user which is the otherclient
      printf("<>joining with %s %s\n", arg1, arg2);
      join_chat_session( one_in_chat(arg2), client_fd, rio_client_fd );

    }
    else if (!strcmp(command, "joinchat") && !strcmp(arg1, "grp")) // joinchat grp <group name>
    {  
      // join the chat session that only have 1 user which is the otherclient
      printf("<> joining group chat %s\n", arg2);
      join_chat_session( get_chat(arg2, true), client_fd, rio_client_fd );

    }
    else if (!strcmp(command, "msg")) /* texting an online user */
    {
        if (is_user_online(arg1))
        { 

          // ask the other user if he/she want to chat
          if (want_to_chat(client, arg1))
          {
            start_chat(client, arg1, rio_client_fd);
          }
          else
          {
            rio_writen(client_fd, "rejected", strlen("rejected") + 1);
          }

        }
        else
        {
          rio_writen(client_fd, "offline", strlen("offline") + 1); 
        }
    }
    else if (!strcmp(command, "users"))
    {   
      char users[MAXLINE];
      users[0] = 0;

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
        printf("%s\n", users );
    }
    else if (!strcmp(command, "grpexist"))
    {   
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
            printf("%s\n", chat_session_arr[i]->session_name);
          }
          sem_post(&read_write_mutex);
        }

        rio_writen(client_fd, users, strlen(users) + 1);
        printf("%s\n", users );
    }
    else if (!strcmp(command, "list") || !strcmp(command, "list\n")) // list <chat_session_name>
    {   
      char users[MAXLINE];
      users[0] = 0;
      int i;

      chat_session_t tmp_chat = get_chat(arg1, false);

      printf("arg1 \n");

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
                printf("%s\n", tmp_chat->users[i]->username);
              }
              sem_post(&read_write_mutex);
          }

          rio_writen(client_fd, users, strlen(users) + 1);
          printf("%s\n", users );
      }
    }

    free(instruct->command);
    free(instruct->arg1);
    free(instruct->arg2);
    free(instruct); 
    
    return;

}


/*
 * * * * * * * * * * * * * * * * * * * * * *
 * read_instruction:                       *
 * in case of success return the command   *
 * name and arguemnt in a struct           *
 * (instruct_t) else return NULL           *
 * * * * * * * * * * * * * * * * * * * * * *
 */
instruct_t read_instruction(char *instruction_buff, size_t buff_size)
{   
    char *command = malloc(MAXLINE);
    char *arg1 = malloc(MAXLINE);
    char *arg2 = malloc(MAXLINE);
    
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
      
    /* Make an instruction struct and put in it the command and the the args */
    instruct_t instruction = malloc(sizeof(struct instruct));

    instruction->command = command;
    instruction->arg1 = arg1;
    instruction->arg2 = arg2;


    return instruction;
}

/*
 * * * * * * * * * * * * * * * * * * * * * *
 * online_user_add:                        *
 * Adding users to online users array      *
 * * * * * * * * * * * * * * * * * * * * * *
 */
void online_user_add(char *user, int client_fd)
{
    int i;

    /* look for an empty spot in the online users array,
       if no empty slot is found, tell the client and close the connection */
    for (i = 0; i < MAX_ONLINE; i++)
    {
                
        // If an empty cache slot is found update it fields
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
    
    /* should be send by client code */ // serverfull
    rio_writen(client_fd, "full",strlen("full") + 1);

    close(client_fd);     
    
    exit(0);
}

/*
 * * * * * * * * * * * * * * * * * * * * * *
 * is_chatting:                            *
 * Client chatting or not                  *
 * * * * * * * * * * * * * * * * * * * * * *
 */
bool is_chatting(char *client, int client_fd)
{
    int i;

    /* look for an empty spot in the online users array,
       if no empty slot is found, tell the client and close the connection */
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
    
    /* should be send by client code */ // serverfull
    rio_writen(client_fd, "full",strlen("full") + 1);

    close(client_fd);     
    
    exit(0);
}

/*
 * * * * * * * * * * * * * * * * * * * * *  *
 * online_user_remove:                        
 * Removing users from the online users array 
 * * * * * * * * * * * * * * * * * * * * * *  
 */
void online_user_remove(char *user, int client_fd)
{
    int i;

    /* look for an empty spot in the online users array,
       if no empty slot is found, tell the client and close the connection */
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
 * * * * * * * * * * * * * * * * * * * * * *
 * is_user_online:                         *
 * Checking if the user is in              *
 * the online users array                  *
 * * * * * * * * * * * * * * * * * * * * * *
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

bool empty_chat_session(chat_session_t chat_session)
{
    int i;
    
    sem_wait(&read_write_mutex);
    user_t *tmp_user = chat_session->users;
    sem_post(&read_write_mutex);

    for (i = 0; i < MAX_ONLINE; i++)
    {   
        sem_wait(&read_write_mutex);
        
        if ( tmp_user[i] != NULL)
        {
          sem_post(&read_write_mutex);
          return true;
        }
        
        sem_post(&read_write_mutex);
    }
    
    return false;
}

/*
 * * * * * * * * * * * * * * * * * * * * * *
 * start_chat:                             *
 * Starting a chat session between         *
 * two clients                             *
 * * * * * * * * * * * * * * * * * * * * * *
 */
void start_chat(char *client, char *otherclient, rio_t rio_client_fd)
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
    join_chat_session(chat, find_client_fd(client), rio_client_fd);

    return;
}

/*
 * * * * * * * * * * * * * * * * * * * * * *
 * join_chat_session:                      *
 * Start sending and receiving messages in *
 * a chat session                          *
 * * * * * * * * * * * * * * * * * * * * * *
 */
// REQUIRES: valid(client_fd) == true
void join_chat_session(chat_session_t chat_session, int client_fd, rio_t rio_client_fd)
{
    //TODO: add the client to the chat_session users
    // keep looping until the user say exit
    // readline, and send to all users in the chat_session
    
    // buffer for reading user messages
    char buf[MAXLINE];
    buf[0] = 0;

    user_t user = get_user(NULL, client_fd);
    
    // Setting user to be in a chatting state
    sem_wait(&read_write_mutex);
    user->chatting = true;
    sem_post(&read_write_mutex);

    int i;
   
   // Add the person to the chat-session-users
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
    // TODO: Only start reading when the chat have at least one user

    printf("Server-Side: %s joinning chat \n", user->username );

    int client_buf_not_empty;

    // Send all msgs the user enter to all people in the chat session, except yourself
    // until the user say exit 
    while ( strcmp(buf ,"exit\n") && strcmp(buf ,"exit") )
    {   
        // Stick the client name at the back every msg, <name> <msg>
      
        /// Recieving Other participants Chat, if there is
        ioctl(client_fd, SIOCINQ, &client_buf_not_empty);
        
        if ( client_buf_not_empty )
        {
          read(client_fd, buf, MAXLINE);

          if ( !strcmp("list\n", buf) || !strcmp("users\n", buf)) // list <chat_session_name>
          {
            strcpy(buf, list_users(chat_session));
            printf("%s \n", buf );
            rio_writen(client_fd, buf, strlen(buf) + 1);
          }
          else if ( strcmp(buf ,"exit\n") && strcmp(buf ,"exit"))
            send_to_chat_session(chat_session, buf);
    

          printf(">> %s\n", buf );

          printf(">>>> %s\n", buf );

    }
        

    }

    printf("%s left chat\n", user->username );
    leave_chat_session(chat_session, client_fd);

}

/*
 * * * * * * * * * * * * * * * * * * * * * *
 * leave_chat_session:                     *
 * Leave a chatting session                *
 * * * * * * * * * * * * * * * * * * * * * *
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
          //free(chat_session->users[i]->username);
          user_t tmp_user = get_user(chat_session->users[i]->username, client_fd);
          tmp_user->chatting = false;

          chat_session->users[i] = NULL;
      }

      sem_post(&read_write_mutex);
  }
}

/*
 * * * * * * * * * * * * * * * * * * * * * *
 * send_to_chat_session:                   *
 * Send a msg to every person in           *
 * the chat session                        *
 * * * * * * * * * * * * * * * * * * * * * *
 */
void send_to_chat_session(chat_session_t chat_session, char *text)
{   
    int i;
    
    int user_fd, text_len_p1 = strlen(text) + 1; 

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
 * * * * * * * * * * * * * * * * * * * * * *
 * is_active_chat:                         *
 * return true when there is               *
 * an active chat session                  *
 * between the two users only, and set     *
 * chat session to point to                *
 * that chat session                       *
 * * * * * * * * * * * * * * * * * * * * * *
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
 * * * * * * * * * * * * * * * * * * * * * *
 * chat_created:                           
 * return a chat session if there is a chat 
 * with only 1 user, that is the user him/herself                                      
 * * * * * * * * * * * * * * * * * * * * * *
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
 * * * * * * * * * * * * * * * * * * * * * *
 * add_chat:                               *
 * make a chat session and add to          *
 * active chats array                      *
 * * * * * * * * * * * * * * * * * * * * * *
 */

chat_session_t add_chat(char *client, bool group_chat, char *session_name)
{
   /* when you add a chat session you join, it and wait for the other
      person to join, since this happen after the acceptance of 
      the chat session, it's expected that the other user find 
      and join this session */

   chat_session_t tmp_chat;

   int i;
   
   // Currently only one-to-one chats are available
   for (i = 0; i < MAX_SESSIONS; i++)
   {
      
      sem_wait(&read_write_mutex);
      tmp_chat = chat_session_arr[i];
      sem_post(&read_write_mutex);

      if ( tmp_chat == NULL)
       {
            // create a chat session and make the first two clients
            // the one provided
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
 * * * * * * * * * * * * * * * * * * * * * *
 * both_in_chat:                           *
 * return true if both users are in        *
 * a one-to-one active chat                *
 * between the two users only, and set     *
 * chat session to point to                *
 * that chat session                       *
 * * * * * * * * * * * * * * * * * * * * * *
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
 * * * * * * * * * * * * * * * * * * * * * *
 * one_in_chat:                            *
 * return true if there is a chat session  *
 * with one active user that is the client *
 * or the person he/she is chatting with   *
 * chat session to point to                *
 * that chat session                       *
 * * * * * * * * * * * * * * * * * * * * * *
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
 * * * * * * * * * * * * * * * * * * * * * *
 * want_to_chat:                           *
 * return true if the client               *
 * want to chat                            *
 * * * * * * * * * * * * * * * * * * * * * *
 */
bool want_to_chat(char *client, char *otherclient)
{
    int otherclient_fd = find_client_fd(otherclient);
    
    char buf[MAXLINE];
    
    rio_t rio_otherclient_fd;

    rio_readinitb(&rio_otherclient_fd, otherclient_fd); 


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
 * * * * * * * * * * * * * * * * * * * * * *
 * group_exist:                           
 * return true iff there exist a chat session 
 * with the given name               
 * * * * * * * * * * * * * * * * * * * * * *
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
 * * * * * * * * * * * * * * * * * * * * * *
 * get_chat:                           
 * return user struct for a chat session with given username               
 * * * * * * * * * * * * * * * * * * * * * *
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
 * * * * * * * * * * * * * * * * * * * * * *
 * get_user:                           
 * return user struct for a client with given username               
 * and/or file descriptor                           
 * * * * * * * * * * * * * * * * * * * * * *
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
                printf("%s\n", chat_users[i]->username);
              }
              sem_post(&read_write_mutex);
          }

    }

    printf("users: %s \n", users );
   
    return users;
}
