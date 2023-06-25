#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PACKET_SIZE 1024
#define PACKET_TYPE 0x11
#define FILE_DELIMITER '&'
#define MAX_PEER_ADDRESS 128

typedef struct {
    char type;
    int len;
} PACKET_HEADER;

typedef struct{
    int id;
    char address[MAX_PEER_ADDRESS];
    int port;
} Peer;

char *save_folder = NULL;
int server_port = 3131;
Peer peers[MAX_PEER_ADDRESS];

int write_file(char *filename, void *data, int data_len) {
  FILE *fp;
  // int len = 0;

  if ((data != NULL) && (data_len > 0)) {
    if ((fp = fopen(filename, "wb")) == NULL) {
      puts("fopen Error!");
      return -1;
    }

    if ((fwrite(data, sizeof(char), data_len, fp)) < data_len) {
      puts("fwrite Error!");
      fclose(fp);
    }
    fclose(fp);
  }

  return 0;
}

ssize_t recv_str(int socket, char* buffer, size_t bufsize) {
  ssize_t bytes_received = recv(socket, buffer, bufsize - 1, 0);
  if (bytes_received >= 0) {
    buffer[bytes_received] = '\0'; // Ensure null-terminated string
  }
  return bytes_received;
}

int save_file(char *file_buf, int filelen) {
  char *split_pos = NULL;
  char fullpath[FILENAME_MAX] = "",
       filename[FILENAME_MAX] = "";
  int filename_len = 0, filecontent_len = 0;

  split_pos = strchr(file_buf, FILE_DELIMITER);

  if (save_folder[strlen(save_folder) - 1] == '/')
    strcpy(fullpath, save_folder);
  else
    sprintf(fullpath, "%s/", save_folder);

  strncpy(filename, file_buf, split_pos - file_buf);
  strcat(fullpath, filename);

  filename_len = strlen(filename);
  filecontent_len = filelen - (filename_len + sizeof(FILE_DELIMITER));

  if (write_file(fullpath, file_buf + strlen(filename) + sizeof(FILE_DELIMITER),
                filecontent_len) != 0) {
    puts("write_file Error!\n");
    return -1;
  }

  printf(" >> Received filename : %s, Filesize : %dByte\n", fullpath,
         filecontent_len);

  return 0;
}

int recv_socket(int sock, char *recv_buf, int recv_len) {
  ssize_t len;
  for (;;) {
    if ((len = recv(sock, recv_buf, recv_len, 0)) > 0){
      return len;
    }
    else
      break;
  }

  return -1;
}

int recv_file(int socket) {
  PACKET_HEADER trans_header;
  char *header = (char *)&trans_header;
  char *file_buf = NULL, *data_buf = NULL;
  int header_len = 0, filelen = 0;
  int read_len = 0, data_len = 0;

  char recv_buf[256];

   int recv_len = recv_socket(socket, recv_buf, sizeof(recv_buf));
   if (recv_len > 0) {
    recv_buf[recv_len] = '\0';
    // printf("Received data: %s\n", recv_buf);

    char *line, *save_ptr;
    int id;
    char ip_buf[INET_ADDRSTRLEN];
    int port;
    int count = 0;
    for (line = strtok_r(recv_buf, "\n", &save_ptr); line; line = strtok_r(NULL, "\n", &save_ptr)) {
        if (sscanf(line, "%d:%[^:]:%d", &id, ip_buf, &port) == 3) {
            if(peers[0].port == port){
              continue;
            }
            else {
              peers[count].id = id;
              strncpy(peers[count].address, ip_buf, MAX_PEER_ADDRESS-1);
              peers[count].address[MAX_PEER_ADDRESS-1] = '\0';
              peers[count].port = port;
              printf("Peer Info -> ID: %d, IP address: %s, port: %d\n", peers[count].id, peers[count].address, peers[count].port);
            }
            count++;
            
        } else {
            printf("Invalid received data format\n");
        }
    }

  } else {
      printf("Failed to receive data\n");
  }

  header_len = 5;
  memset(&trans_header, 0, sizeof(PACKET_HEADER));
  while ((read_len =
             recv_socket(socket, (char *)header, header_len))!=0) {
    if (read_len == -1)
      return -1;
    else if (read_len == header_len)
      break;

    header_len -= read_len;
    header += read_len;
  }
  memcpy(&trans_header.len, &header[1], 4);
  filelen = ntohl(trans_header.len);

  if ((filelen <= 0) || (trans_header.type != PACKET_TYPE))
    return -1;

  if ((file_buf = (char *)calloc(1, filelen + 1)) == NULL)
    return -1;

  read_len = 0;
  data_buf = file_buf;
  data_len = filelen;
  while(1) {
    read_len = recv_socket(socket, (char *)data_buf, data_len);
    if (read_len == -1) {
      puts("recv_socket Error!");
      return -1;
    } else if (read_len == data_len)
      break;

    data_len -= read_len;
    data_buf += read_len;
  }

  save_file(file_buf, filelen);

  free(file_buf);
  return 0;
}

int recv_packet(char* server_ip, int server_port) {
    int sock;
    struct sockaddr_in server_address;

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        puts("socket Error!");
        return -1;
    }

    memset((char*)&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(server_ip);
    server_address.sin_port = htons(server_port);
    if (connect(sock, (struct sockaddr*)&(server_address),
                sizeof(server_address)) < 0) {
        puts("connect Error!");
        return -1;
    }


    // client_addr_len = sizeof(client_addr);
    // if(getsockname(sock, (struct sockaddr*)&client_addr, &client_addr_len) == 0){
    //   char client_ip[INET_ADDRSTRLEN];
    //   int client_port = ntohs(client_addr.sin_port);
    //   inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    //   // peers[0].id = 1;
    //   // strncpy(peers[0].address, client_ip, MAX_PEER_ADDRESS-1);
    //   // peers[0].address[MAX_PEER_ADDRESS-1] = '\0';
    //   // peers[0].port = client_port;
    //   // // printf("Current My Client ID %d, IP Address: %s, Port: %d\n", peers[0].id, client_ip, client_port);
    //   // printf("Current My Client ID %d, IP Address: %s, Port: %d\n", peers[0].id, peers[0].address, peers[0].port);
    // }

    puts(" >> Client connected to Server!!");

    if (recv_file(sock) < 0){
      puts("client_recv Error\n");
      return -1;
    }

    close(sock);
    return 0;
}

int main(int argc, char* argv[]) {
    char *server_ip = NULL;
    int server_port = 1234;
    char *filebuf = NULL, *packet_buf = NULL;

    if (argc != 4) {
        puts("Instructions : fileclient [ServerIP][ServerPort][Path]\n");
        puts("     ex) fileclient 203.252.102.25 3131 /not_boost_\n\n");
        exit(0);
    }

    server_ip = argv[1];
    server_port = atoi(argv[2]);
    save_folder = argv[3];
 
    if (recv_packet(server_ip, server_port) != 0) {
        puts("recv_packet Error!!");
        free(filebuf);
        free(packet_buf);
    } else
        puts(" >> Received file complete!!");

    free(filebuf);
    free(packet_buf);
    return 0;
}
