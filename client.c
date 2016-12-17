#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>    /* for getopt */
#include <stdio_ext.h>

#define MAXLINE 1024   

// Color Escape Codes
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"



// Created by: Julian Sam on 17th December 2016.

void usage(void); // Prints info about app
void showCommands(void); // Show commands for user


int main(int argc, char **argv)
{
	char c, verbose;
	// Get username. (Assumption for CMU logins only)
    char buf[MAXLINE];
    char* name = getenv("LOGNAME"); 

	while ((c = getopt(argc, argv, "hvp")) != EOF) {
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
    printf(GREEN "Welcome to Unix ChatBox, " RESET);
    printf(CYAN "%s \n" RESET, name);
    printf("Created By Abubaker Omer, Julian Sam and Mohammed Hashim.\n");
    printf("Type !h for help\n");
    printf("Type \"c\" to see possible commands\n");



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

		else if (strlen(buf) == 5 && !strcmp("exit\n",buf)) // Exit client
			exit(0);

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
    printf("   \"users\" : Show users on this machine\n");
    printf("   \"chat\"  : Start a chat with a user\n");
    printf("   \"group\" : Start a group chat\n");
    printf("   \"exit\"  : Exit\n");
    return;
}