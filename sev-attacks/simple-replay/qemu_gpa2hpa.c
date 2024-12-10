#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <regex.h>

#include "helpers.h"


/**
 * @brief parse the hpa from the unstructured message in `response`
 * @param response: Out of the QEMU hdm command `gpa2hva`
 * @pram out_hpa: Output param. Filled with the parsed hpa.
 * @returns 0 on success
*/
int parse_gpa2hpa_response(char* response, uint64_t* out_hpa) {
  regex_t re;
  size_t groups_len = 3;
  regmatch_t groups[groups_len];

  char regex[] = R"(^\{"return": "Host physical address for (0x[0-9a-f]+) .*(0x[0-9a-f]+))";
  if( regcomp(&re, regex, REG_EXTENDED)) {
    err_log( "failed to parse static regexp, this should never happend\n");
    regfree(&re);
    return -1;
  }
  if( regexec(&re, response, groups_len, groups, 0) ) {
    err_log("failed to match regexp\n");
    regfree(&re);
    return -1;
  }

  //idx 0 : whole match, idx 1: gpa: idx 2 hpa
  uint64_t hpa;
  if( groups[2].rm_so == - 1 ) {
    err_log("did not get match grou group 2\n");
    return -1;
  }
  size_t match_len = groups[2].rm_eo -groups[2].rm_so;
  char* match = malloc(match_len);
  strncpy(match, response + groups[2].rm_so, match_len);
  match[match_len] = '\0';
  //printf("match is: rm_so=%d, rm_eo=%d, string=%s\n", groups[2].rm_so, groups[2].rm_eo, match);
  if( do_stroul(match, 16, &hpa)) {
    err_log("failed to parse to uint64_t\n");
    regfree(&re);
    free(match);
    return -1;
  }
  free(match);

  *out_hpa = hpa;
  return 0;
}

int qemu_gpa_to_hpa(uint64_t gpa, char* qmp_ip, uint16_t port, uint64_t* out_hpa) {
  int socket_fd = -1;
  size_t buf_bytes = 1024;
  const size_t qmp_cmd_gpa2hpa_maxlen = 256;
  char qmp_cmd_gpa2hpa[qmp_cmd_gpa2hpa_maxlen];
  uint8_t* buf = malloc(buf_bytes);
  struct sockaddr_in server_address;


  if( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0 ) {
    err_log("failed to connect to qmp socket: %s", strerror(errno))
    return -1;
  }


  //prepare address information struct
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);

  if (inet_pton(AF_INET, qmp_ip, &server_address.sin_addr) <= 0) {
    err_log("inet_pton failed: %s\n", strerror(errno));
    goto error;;
  }

  // Connect to server
  printf("Trying to connect to %s:%d\n", qmp_ip, port);
  if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
    err_log("failed to connect to %s:%d : %s\n", qmp_ip, port, strerror(errno));
    goto error;  
  }

  //printf("Connected to server\n");

  /*
   * Interacte with qmp interface
   * 1) Read "Hello Message" from server
   * 2) Send message to switch to cmd mode
   * 3) Read ack
   * 4) Send gpa2hpa command
   * 5) read result
  */


  //1) read "hello message"
  int read_bytes = read(socket_fd, buf, buf_bytes);
  if( -1 ==read_bytes ) {
    err_log("failed to read hello msg from qmp :%s\n", strerror(errno));
    goto error;
  }
  buf[read_bytes] = '\0';
  //printf("Server said: %s\n",(char*) buf);

  //2) switch to cmd mode
  //printf("Sending activate cmd\n");
  const char* qmp_cmd_activate = "{ 'execute': 'qmp_capabilities' }";
  int send_bytes = send(socket_fd, qmp_cmd_activate, strlen(qmp_cmd_activate), 0);
  if( -1 == send_bytes || strlen(qmp_cmd_activate) != (unsigned long)send_bytes) {
    err_log("failed to send qmp activate cmd : %s", strerror(errno));
    goto error;
  }


  //3) read ack
  read_bytes = read(socket_fd, buf, buf_bytes);
  if( -1 ==read_bytes ) {
    err_log("failed to read ack msg from qmp : %s\n", strerror(errno));
    goto error;
  }
  buf[read_bytes] = '\0';
  //printf("Server said: %s\n",(char*) buf);

  //4) send gpa2hpa command
  //printf("Sending gpa2hpa command\n");
  int n = snprintf(qmp_cmd_gpa2hpa, qmp_cmd_gpa2hpa_maxlen, "{'execute': 'human-monitor-command', 'arguments': {'command-line': 'gpa2hpa 0x%jx'}}", gpa);
  if( n == -1 || (size_t)n >= qmp_cmd_gpa2hpa_maxlen ) {
    err_log("failed to craft qmp activate command: snprintf retunred %d : %s\n",n, strerror(errno));
    goto error;
  }
  send_bytes = send(socket_fd, qmp_cmd_gpa2hpa, strlen(qmp_cmd_gpa2hpa), 0);
  if( -1 == send_bytes || strlen(qmp_cmd_gpa2hpa) != (unsigned long)send_bytes) {
    err_log("failed to send qmp gpa2hpa command : %s", strerror(errno));
    goto error;
  }

  //5) read result
  read_bytes = read(socket_fd, buf, buf_bytes);
  if( -1 ==read_bytes ) {
    err_log("failed to read ack msg from qmp : %s\n", strerror(errno));
    goto error;
  }
  buf[read_bytes] = '\0';
  printf("Raw QMP response: %s\n",(char*) buf);

  //printf("parsing hpa\n");
  uint64_t hpa;
  if( parse_gpa2hpa_response((char*)buf, &hpa) ) {
    err_log("failed to parse response : %s\n", strerror(errno));
    goto error;
  }
  *out_hpa = hpa;

  
  int ret = 0;
  goto clenaup;
error:
    ret = - 1;
clenaup:
  if( socket_fd != -1 ) {
    close(socket_fd);
  }
  if(buf) {
    free(buf);
  }
  return ret;
}

/*int main() {
  uint64_t hpa;
  if( qemu_gpa_to_hpa(0x6d0a3000, "127.0.0.1", 4444, &hpa) ) {
    printf("qemu_gpa2hpa failed\n");
    return -1;
  }
  printf("hpa is 0x%jx\n", hpa);

  return 0;
}*/
