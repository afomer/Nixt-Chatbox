#include <time.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h>
#include <curl/curl.h>

struct string_holder {
  char *ptr;
  size_t len;
};

void init_string(struct string_holder *s) {
  s->len = 0;
  s->ptr = malloc(s->len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

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


char* getTimeZone(char* timeString)
{

  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();
  if(curl) {
    struct string_holder s;
    init_string(&s);
    int i;
    char host[999999];
    strcpy(host,"http://geoip.nekudo.com/api/");

    strcat(host,timeString);

    curl_easy_setopt(curl, CURLOPT_URL, host);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
    res = curl_easy_perform(curl);


    char* finalString = malloc(sizeof(char)*s.len);
    for ( i = 0; i < s.len; i++)
    {
      finalString[i] = s.ptr[i];
    }
    // printf("%s\n", s.ptr);
    free(s.ptr);

    // printf("%s\n", finalString);
    /* always cleanup */
    curl_easy_cleanup(curl);
    return finalString;
  }
  return NULL;
}



char* getfromBash(char* timezone)
{
  char bash_cmd[256] = "TZ=";
  strcat(bash_cmd,timezone);
  strcat(bash_cmd," date");

  char buffer[1000];
  FILE *pipe;
  int len; 

  pipe = popen(bash_cmd, "r");

  if (NULL == pipe) {
      perror("pipe");
      exit(1);
  } 

  fgets(buffer, sizeof(buffer), pipe);

  len = strlen(buffer);
  buffer[len-1] = '\0'; 
  printf("%s\n",buffer);

  char* timeString;
  timeString = strtok(buffer," ");
  timeString = strtok(NULL," ");
  timeString = strtok(NULL," ");
  timeString = strtok(NULL," ");
  printf("%s\n", timeString);
  pclose(pipe);

  return timeString; // ADD MALLOC. ISSUES hERE
}

int main()
{
	time_t rawtime;
	struct tm * timeinfo;
	char* timeString;
	char* splitString;
  getfromBash();
	char* name = getenv("SSH_CLIENT"); 
	if (name == NULL)
	{
		time ( &rawtime );
		timeinfo = localtime ( &rawtime );
		char* splitString = asctime(timeinfo);
		timeString = strtok(splitString," ");
		timeString = strtok(NULL," ");
		timeString = strtok(NULL," ");
		timeString = strtok(NULL," ");
		printf("%s\n", timeString);
	}
	else
	{
		printf("%s\n",name);
    timeString = strtok(NULL," ");
    timeString = strtok(NULL," ");
    timeString = getTimeZone(timeString);
	}

  // return timeString
	return 0;
}
