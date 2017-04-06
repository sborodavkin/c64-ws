#include <c64.h>
#include <cbm.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


////////////////////////////////////////////////////////////////////////////////
// GLOBALS
////////////////////////////////////////////////////////////////////////////////

#define poke(A,X)      (*(unsigned char *)A) = (X)
#define peek(A)        (*(unsigned char *)A)
#define poke_word(A,X) (*(unsigned int *)A) = (X)
#define peek_word(A)   (*(unsigned int *)A)

// Explicitly specify RS232 baud rate and mode as byte array instead of string,
// as CC65 auto-translates strings from ASCII to PETSCII.
const unsigned char NAME300[] = {0x06, 0x00, 0x00};

// RS232 device number.
const unsigned int RS232_HANDLER = 2;

// The RS-232 driver expects to have 256-byte buffers for input and output.
char RS232_READ_BUF[512];
char RS232_WRITE_BUF[512];

// These memory locations store pointers to the RS-232 read
// and write buffers. We=E2=80=99ll fill them in with pointers to
// our read and write buffers before we open the RS-232
// device. That=E2=80=99ll prevent the KERNAL open routine from
// allocating buffers from the top of BASIC memory, which
// would probably really mess us up.
char** RIBUF = (char**)0x00f7;
char** ROBUF = (char**)0x00f9;

// GETIN - get a character from default input device, missing in cbm.h
unsigned char __fastcall__ (*cbm_k_getin)(void) = 0xffe4;
unsigned char __fastcall__ (*cbm_k_clr)(void) = 0xe566;


// Request-related defs.

// HTTP request buffer.
#define MAX_REQUEST_SIZE 4096
static char REQUEST_BUF[MAX_REQUEST_SIZE];

static char REQUEST_METHOD_GET = 1;
static char REQUEST_METHOD_POST = 2;

const char PARSE_RESULT_OK = 0;
const char PARSE_RESULT_WRONG_METHOD = 1;
const char PARSE_RESULT_NO_URL = 2;
const char PARSE_RESULT_NO_LINE = 3;

struct http_request {
  char method;
  char url[255];
};

static char HTTP_REQUEST_FIRST_LINE[255];
static struct http_request PARSED_REQUEST;

// Response-related defs.

const char* HTTP_RESPONSE_CODE_200 = "200 ok";
const char* HTTP_RESPONSE_CODE_400 = "400 bad request";
const char* HTTP_RESPONSE_CODE_404 = "404 not found";
const char* HTTP_RESPONSE_CODE_500 = "500 server error";

// When something goes wrong better hardcode this.
const char* HTTP_RESPONSE_SERVER_ERROR = "http/1.1 500 server error\n\n";


#define MAX_RESPONSE_SIZE 2048

const char RESPONSE_OK = 0;
const char RESPONSE_TOO_LONG_FILENAME = 1;
const char RESPONSE_NOT_FOUND = 2;
const char RESPONSE_FILE_OPEN_PROBLEM = 3;
const char RESPONSE_FILE_READ_PROBLEM = 4;


////////////////////////////////////////////////////////////////////////////////

// Converts the input_str to lowercase (replacing the original).
char* to_lower(char* input_str) {
  int i = 0;
  while(input_str[i]) {
    input_str[i] = tolower(input_str[i]);
    i++;
  }
  return input_str;
}

// Prints the intro text.
void print_intro() {
  cbm_k_clr();
  printf("**************************************\n");
  printf("*        C64-BASED HTTP SERVER       *\n");
  printf("*    (C) SERGEY BORODAVKIN, 2017.    *\n");
  printf("**************************************\n\n");
}

// Opens RS232 and returns 0 if success.
unsigned int open_rs232() {
  int res = 0;
  printf("Opening RS232...\n");
  // Set up rs-232 buffers.
  *RIBUF = (char*)(((int)RS232_READ_BUF & 0xff00) + 256);
  *ROBUF = (char*)(((int)RS232_WRITE_BUF & 0xff00) + 256);

  // Open rs232 channel.
  cbm_k_setlfs(RS232_HANDLER, 2, 3);
  cbm_k_setnam (NAME300);
  res = cbm_k_open();
  cbm_k_chkin(RS232_HANDLER);
  return res;
}

// Check if buffer ends with an empty line (either \n\n or \r\n\r\n).
bool _is_request_end(char* buffer, unsigned int size) {
  if (size >= 4) {
    // Can be either 2* \n or 2* \r\n.
    if ((buffer[size-1] == '\n' && buffer[size-2] == '\n') ||
        (buffer[size-1] == '\n' && buffer[size-3] == '\n')) {
      return true;
    } else {
      return false;
    }
  } else if (size >= 2) {
    // Can only be \n\n.
    return (buffer[size-1] == '\n' && buffer[size-2] == '\n');
  }
  return false;
}

// Parses the HTTP request from char* into struct http_request.
char _parse_request(char* request, struct http_request* result) {
  const char *first_line_end;
  char* space_1_idx;
  char* space_2_idx;

  first_line_end = strchr(request, '\n');
  if (first_line_end) {
    strncpy(HTTP_REQUEST_FIRST_LINE, request, first_line_end-request);
    to_lower(HTTP_REQUEST_FIRST_LINE);
    if (HTTP_REQUEST_FIRST_LINE[1] == 'g' &&
        HTTP_REQUEST_FIRST_LINE[2] == 'e' &&
        HTTP_REQUEST_FIRST_LINE[3] == 't' &&
        HTTP_REQUEST_FIRST_LINE[4] == ' ') {
      // GET request.
      result->method = REQUEST_METHOD_GET;
    } else {
      if (HTTP_REQUEST_FIRST_LINE[1] == 'p' &&
          HTTP_REQUEST_FIRST_LINE[2] == 'o' &&
          HTTP_REQUEST_FIRST_LINE[3] == 's' &&
          HTTP_REQUEST_FIRST_LINE[4] == 't' &&
          HTTP_REQUEST_FIRST_LINE[5] == ' ') {
        // POST request.
        result->method = REQUEST_METHOD_POST;
      }
    }
    if (result->method) {
      space_1_idx = strchr(HTTP_REQUEST_FIRST_LINE, ' ');
      if (space_1_idx) {
        space_2_idx = strchr(space_1_idx+1, ' ');
        if (space_2_idx) {
          *space_2_idx = 0; // ignore HTTP version
          strncpy(result->url, space_1_idx+1, (int)(space_2_idx-space_1_idx));
          return PARSE_RESULT_OK;
        } else {
          return PARSE_RESULT_NO_URL;
        }
      } else {
        return PARSE_RESULT_NO_URL;
      }
    } else {
      return PARSE_RESULT_WRONG_METHOD;
    }
  }
  return PARSE_RESULT_NO_LINE;
}

// Deals with request and creates the response string.
// Memory for response must be pre-allocated.
char _create_response(const struct http_request* request, char* response) {
  // 16 (max on C64) + 1 byte for \0 + 1 byte for \0 + 5 for disk ID and mode.
  unsigned char* filename = (unsigned char*) malloc(16+1+5);  
  unsigned char* file_data = (unsigned char*) malloc(1024);
  int file_size = -1;
  int response_code = RESPONSE_OK;
  int open_code = 0;
  int offset = 0;
  int i = 0;
  char* content_length_str = "    ";  // Helper string to store CL as string.

  sprintf(response, "http/1.1 ");
  if (strlen(request->url) > 17) {
    printf("%s is too long request URL\n", request->url);
    strcat(response, HTTP_RESPONSE_CODE_400);
    strcat(response, "\r\n");
    response_code = RESPONSE_TOO_LONG_FILENAME;
  } else {
    // Check for "GET / HTTP/1.1" - request index.htm by default.
    if (strlen(request->url) == 1) {
      sprintf(filename, "0:index.htm,s,r");
    } else {
      // +1 to trim leading '/'
      sprintf(filename, "0:%s,s,r", to_lower(request->url+1));
    }
    printf("Opening %s...\n", filename);
    open_code = cbm_open(6, 8, CBM_READ, filename);
    if (open_code != 0) {
      if (open_code == 4) {
        printf("Not found.\n");
        strcat(response, HTTP_RESPONSE_CODE_404);
        response_code = RESPONSE_NOT_FOUND;
      } else {
        printf("Open error code=%d\n", open_code);
        strcat(response, HTTP_RESPONSE_CODE_400);
        response_code = RESPONSE_FILE_OPEN_PROBLEM;
      }
      strcat(response, "\r\n");
    } else {
      printf("Reading from file...\n");
      file_size = cbm_read(6, file_data, 2048);
      printf("%d bytes read.\n", file_size);
      if (file_size == -1 || !file_size) {
        // Generic error during file read.
        printf("Error read file, _oserror=%d", _oserror);
        strcat(response, HTTP_RESPONSE_CODE_500);
        response_code = RESPONSE_FILE_READ_PROBLEM;
        strcat(response, "\r\n");
      } else {
        sprintf(content_length_str, "%d", file_size);
        
        strcat(response, HTTP_RESPONSE_CODE_200);
        strcat(response, "\r\n");
        strcat(response, "content-length: ");        
        strcat(response, content_length_str);
        strcat(response, "\r\n");
        offset = strlen(response);
        for (i = 0; i < file_size; i++) {
          *(response + offset + i) = *(file_data + i);
        }
        *(response + offset + i) = '\r';
        *(response + offset + i + 1) = '\n';
        *(response + offset + i + 2) = 0;        
      }
      cbm_close(6);  // Close the file only if it has been opened.
    }
  }
  free(filename);
  free(file_data);  
  
  return response_code;
}

void _handle_request(char* request, unsigned int size) {
  unsigned int i = 0;
  char parse_code;
  char response_code;
  char* http_response;
  printf("Serving request:\n");
  for(; i < size; i++) {
    putchar(request[i]);
  }
  parse_code = _parse_request(request, &PARSED_REQUEST);
  if (parse_code == PARSE_RESULT_OK) {
    http_response = (unsigned char*) malloc(MAX_REQUEST_SIZE);
    if (http_response) {
      response_code = _create_response(&PARSED_REQUEST, http_response);
      printf(http_response);
      free(http_response);
    }
  } else {
    printf("Error parsing request, code=%d\n", (int)parse_code);
  }
}

// Main loop of the app.
void main_loop() {
  unsigned int cur_char_idx = 0;
  unsigned char c;
  unsigned int PROGRESS_STEP = 20;  // How often to update the progress bar.
  bool is_new_request = true;
  for (;;) {
    if (cur_char_idx >= MAX_REQUEST_SIZE) {
      printf ("\n!!! Request too large, throttling !!!\n");
      cur_char_idx = 0;
      is_new_request = true;
    }
    cbm_k_chkin(RS232_HANDLER);
    c = cbm_k_getin();
    if (c) {
      REQUEST_BUF[cur_char_idx++] = c;
      if ((cur_char_idx-1) % PROGRESS_STEP == 0) {
        if (is_new_request) {
          printf ("\nStart getting request: [");
          is_new_request = false;
        }
        printf (".");
      }
      if (c == '\n' && _is_request_end(REQUEST_BUF, cur_char_idx)) {
        printf ("]\n");
        REQUEST_BUF[cur_char_idx] = 0;  // Make 0-terminated string.
        _handle_request(REQUEST_BUF, cur_char_idx);
        cur_char_idx = 0;
        is_new_request = true;
      }
    }
  }
}

int main (void) {
  unsigned int rs232_open_code;

  // Switch to ALL CAPS (cc65 switches to lowercase in startup code).
  // poke(0xd018, 0x15);

  print_intro();
  if ((rs232_open_code = open_rs232()) > 10) {
    printf("Starting main loop.\n");
    main_loop();
  } else {
    printf("Error opening RS232, code %d", rs232_open_code);
  }



  /*
  for (;;)
  {
    // look for a keyboard press
    // cbm_k_chkin (0);
    // c = cbm_k_getin();
    // if (c)
    //{
    //putchar(c);
    //cbm_k_ckout(2);
    //cbm_k_bsout(c);
    //}

    // look for input on rs232
    cbm_k_chkin(2);
    c = cbm_k_getin ();
    if (c)
    {
      putchar(c);
    }
  }
  */
  return EXIT_SUCCESS;
}
