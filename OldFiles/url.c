#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int main(void)
{
  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();
  if(curl) {
    struct string_holder s;
    init_string(&s);
    int i;
    curl_easy_setopt(curl, CURLOPT_URL, "http://geoip.nekudo.com/api/8.8.8.8");
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
  }
  return 0;
}