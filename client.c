#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio_ext.h>

#define MAXLINE 1024   

// Color Escape Codes
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m" // Reset color code



// Created by: Julian Sam on 17th December 2016.

void usage(void); // Prints info about app
void showCommands(void); // Show commands for user
void printUsers(void); // Show all users
void startChat();
void startGroup();
void printHelpInfo(); // prints help info: add later


int main(int argc, char **argv)
{
	char c, verbose;
	// Get username. (Assumption for CMU logins only)
    char buf[MAXLINE];
    char* name = getenv("LOGNAME"); 

	while ((c = getopt(argc, argv, "hv")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
            break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
            break;
        default:
            usage();
        }
    }

    (void)verbose; // to avoid compiler warning. Will be used later.
    printf(GREEN "Welcome to the Unix ChatBox, " RESET);
    printf(CYAN "%s \n" RESET, name);
    printf("Created By Abubaker Omer, Julian Sam and Mohammed Hashim.\n");
    // printf("Type \"h\" for help\n");
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
					|| (!strcmp("q\n",buf) && strlen(buf) == 2)) // Exit client
			exit(0);

		else if (!strcmp("chat\n",buf) && strlen(buf) == 5) // Exit client
			startChat();

		else if (!strcmp("group\n",buf) && strlen(buf) == 6) // Exit client
			startGroup();

		else if (!strcmp("users\n",buf) && strlen(buf) == 6 )
			printUsers();


		else {
			printf("%s", buf); // Print back given input (echo it)
			fflush(stdout); // Flush to screen
		}
	}
	exit(0);
}

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hva]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
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