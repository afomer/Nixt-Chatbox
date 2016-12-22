#include "jsmn/jsmn.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// JSMN Library is from https://github.com/zserge/jsmn

int main(int argc, char *argv[])
{   
	// Set the number of tokens in the json (key AND value)
	int token_num = 20;

	jsmn_parser parser;

	jsmntok_t tokens[token_num];

	jsmn_init(&parser);
    
    // Put your JSON HERE
    char *js = "{\"city\":\"Doha\",\"country\":{\"name\":\"Qatar\",\"code\":\"QA\"},\"location\":{\"accuracy_radius\":1,\"latitude\":25.2867,\"longitude\":51.5333,\"time_zone\":\"Asia\/Qatar\"},\"ip\":\"37.211.46.183\"}";
	
	// Run the JSON parser, JSMN
	jsmn_parse(&parser, js, strlen(js), tokens, token_num);
	
	// timezone should be the 19th token
	jsmntok_t timezone_token = tokens[18];
	
	// get the length of the string, by getting the ending and starting postions
	unsigned int str_length = timezone_token.end - timezone_token.start;
	
	// make a string buffer for the timezone and copy the string to that buffer
	// using memcpy
	char timezone_str[str_length + 1]; // +1 for '\0'
	
	memcpy(timezone_str, &js[timezone_token.start], str_length);
	
	// >> your timezone string
	timezone_str[str_length] = '\0';

	

	return 0;
}