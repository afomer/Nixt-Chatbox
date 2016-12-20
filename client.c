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
#include <stdio_ext.h>
#include <time.h>
#include <limits.h>
#include <curl/curl.h>

#define MAXLINE 1024   

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
void printUsers(void); // Show all users
void startChat();
void startGroup();
void printHelpInfo(); // prints help info: add later


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
    char* name = getenv("LOGNAME"); // Gets Username from env. variable

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


    printf(GREEN "Welcome to the Unix ChatBox, " RESET);
    printf(CYAN "%s \n" RESET, name);
    printf("Created By Abubaker Omer, Julian Sam and Mohammed Hashim.\n");
    printf("Type " YELLOW "\"c\"" RESET " to see possible commands\n");

	while (1) {
		if (feof(stdin)) { /* End of file (ctrl-d) */
			printf ("\nGoodBye!\n");
		    exit(0);
		}

		printf(">> "); // Type Prompt Symbol 
		fflush(stdout); // Flush to screen
		fgets (buf, MAXLINE, stdin); // Read command line input


		if (strlen(buf) == 2 && buf[0] == 'c') // Show commands
			showCommands();

		else if (((!strcmp("exit\n",buf) || !strcmp("quit\n",buf))
					&& strlen(buf) == 5)
					|| (!strcmp("q\n",buf) && strlen(buf) == 2))// Exit client
		{
			if (SSH == true) // Free malloc'ed strings before exiting.
			{
				free(timezone);
				free(splitString);
			}	
			exit(0);
		}

		else if (!strcmp("chat\n",buf) && strlen(buf) == 5) 
			startChat();

		else if (!strcmp("group\n",buf) && strlen(buf) == 6) 
			startGroup();

		else if (!strcmp("users\n",buf) && strlen(buf) == 6 )
			printUsers();


		else {
			// Print Current Time
			if (SSH == false)
			{
				// IF NOT SSH'ed, then just print local system time:
				time ( &rawtime );
				timeinfo = localtime ( &rawtime );
				splitString  = asctime(timeinfo);

				// Split string to get just the time.
				timeString = strtok(splitString," ");
				timeString = strtok(NULL," ");
				timeString = strtok(NULL," ");
				timeString = strtok(NULL," ");
			}
			else
			{
				// Initialize variables to get original time
				char bash_cmd[256] = "TZ=";
				strcat(bash_cmd,timezone);
				strcat(bash_cmd," date");
				// Open pipe to bash to retrieve time.
				pipe = popen(bash_cmd, "r");
				if (NULL == pipe) {
					perror("pipe");
					exit(1);
				} 
				// Ask bash shell for the ssh client time based on timezone:
				fgets(buffer, sizeof(buffer), pipe);
				len = strlen(buffer);
				buffer[len-1] = '\0'; 

				// Split string to get time only.
				timeString = strtok(buffer," ");
				timeString = strtok(NULL," ");
				timeString = strtok(NULL," ");
				timeString = strtok(NULL," ");
				pclose(pipe);
			}
			printf(YELLOW"[%s] " GREEN "%s: " RESET "%s", timeString, name, buf); 
								// Print back given input (echo it)
			fflush(stdout); // Flush to screen
		}
	}

	exit(0); // Should never reach here.
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

void printUsers(void)
{
	printf("USERS:\n");
}

void startChat()
{
	char name[MAXLINE]; // Name of user
	printf("Enter Name/ID: ");
	fflush(stdout);
	fgets (name, MAXLINE, stdin); // Read command line input

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

  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();
  if(curl) {
    struct string_holder s;
    init_string(&s);
    int i;
    char host[10000];
    // Use this website to send IP address and get back time zone.
    strcpy(host,"http://geoip.nekudo.com/api/");
    strcat(host,IP);

    curl_easy_setopt(curl, CURLOPT_URL, host);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
    res = curl_easy_perform(curl);
    (void)res;
    char* finalString = malloc(sizeof(char)*s.len);
    for ( i = 0; i < s.len; i++)
    {
      finalString[i] = s.ptr[i]; // Copy the string to a malloced string.
    }
    free(s.ptr);
    curl_easy_cleanup(curl);

    return finalString;
  }
  return NULL;
}
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

