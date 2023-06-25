#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>


#define PACKET_SIZE 1024
#define PACKET_TYPE 0x11
#define FILE_DELIMITER '&'
#define MAX_CLIENTS 3
#define MAX_PEER_ADDRESS 128

typedef struct{
    int id;
    char address[MAX_PEER_ADDRESS];
    int port;
} Peer;

typedef struct {
  char type;
  int len;
} PACKET_HEADER;

char *save_folder = NULL;
int server_port = 3131;
char *file_name = NULL;
pthread_mutex_t client_count_lock;
int client_count = 0;
int client_sockets[MAX_CLIENTS];
Peer peers[MAX_PEER_ADDRESS];
int peer_count = 0;
int client_conn_count = 0;

int add_peer(int id, char* address, int port){
  int i = 0;
  for(i = 0; i<MAX_CLIENTS; i++){
      if(!peers[i].id){
          peers[i].id = id;
          strncpy(peers[i].address, address, MAX_PEER_ADDRESS-1);
          peers[i].address[MAX_PEER_ADDRESS-1] = '\0';
          peers[i].port = port;
          peer_count++;
          return 0;
      }
  }
  return - 1;
}
int remove_peer(int id) {
  int i = 0;
  for (i = 0; i < MAX_CLIENTS; ++i) {
    if (peers[i].id == id) {
      peers[i].id = 0;
      peer_count--;
      return 0;
    }
  }
  return -1;  // if peer not found
}

void print_peers() {
  printf("List of peers:\n");
  int i = 0;
  for (i = 0; i < MAX_CLIENTS; ++i) {
    if (peers[i].id) {
      printf("%d: address : %s, port : %d\n", peers[i].id, peers[i].address, peers[i].port);
    }
  }
}

ssize_t send_str(int socket, const char* str) {
  return send(socket, str, strlen(str) + 1, 0);
}
void send_peer_list(int client_socket) {
  char send_buf[PACKET_SIZE + 1];
  int packet_len = 0;
  // int len = 0;
  memset(send_buf, 0, sizeof(send_buf));

  // Create the peer list string
  int i = 0;
  for (i = 0; i < MAX_CLIENTS; ++i) {
    if (peers[i].id) {
      packet_len += snprintf(send_buf + packet_len, PACKET_SIZE - packet_len, "%d:%s:%d\n", peers[i].id, peers[i].address,peers[i].port);
      // printf("packet_len : %d\n", packet_len);
      if (packet_len >= PACKET_SIZE) {
        // List truncated, close connection
        puts("send_peer_list Error");
        close(client_socket);
        return;
      }
    }
  }

  printf("saved_list : \n%s\n",send_buf);
  // Send the peer list string

  if (send_str(client_socket, send_buf) < 0) {
    puts("Error sending peer list");
  }
  sleep(1);

}

int get_filelen(char* filename) {
    FILE* fp = NULL;
    int filelen;

    if ((fp = fopen(filename, "rb")) == NULL) {
        puts("fopen Error!");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    filelen = ftell(fp);

    fclose(fp);
    return filelen;
}

int read_file(char* path, char* buf, int len) {
    FILE* fp = NULL;
    int readlen;

    if ((fp = fopen(path, "rb")) == NULL) {
        puts("fopen Error!");
        return -1;
    }
    buf[len] = NULL;
    if ((readlen = fread(buf, sizeof(char), len, fp)) < len) {
        fclose(fp);
        puts("fread Error!");
        return -1;
    }

    fclose(fp);
    return 0;
}
int send_file(int socket, char packet_type, char* packet_buf, int packet_len){
  char send_buf[PACKET_SIZE + 1];
  int p_len = 0, send_len = 0;

  memset(send_buf, 0, sizeof(send_buf));

  p_len = (long)htonl((long)packet_len);
  send_buf[0] = packet_type;

  memcpy(send_buf + 1, &p_len, sizeof(p_len));
  send_len = send(socket, send_buf, 5, 0);


  while(1){
    memset(send_buf, 0, sizeof(p_len));
    if(packet_len <= 0){
      break;
    }
    
    if(packet_len <= PACKET_SIZE){ 
      memcpy(send_buf, packet_buf, packet_len);
      if((send_len = send(socket, send_buf, packet_len, 0)) < 0){
        return -1;
      }
      if(send_len == packet_len){
        break; 
      }
      packet_buf += send_len; 
      packet_len -= send_len; 
    }
    else {
      memcpy(send_buf, packet_buf, PACKET_SIZE);
      if((send_len = send(socket, send_buf, PACKET_SIZE, 0)) < 0){
        return -1;
      }
      packet_buf += send_len;
      packet_len -= send_len;
    }
  }
  return 0;
}

void *fileserver_main(void *arg) {
  // int sock = (intptr_t)arg;
  char *filebuf = NULL, *packet_buf = NULL;
  int file_len = 0, packet_len = 0;
  int client_socket = 0;
  int len = 0;
  int i = 0;
  // int client_port = 0;
  file_len = get_filelen(file_name);

  if ((filebuf = (char*)calloc(1, file_len + 1)) == NULL) {
    pthread_exit(NULL);
  }
  if (read_file(file_name, filebuf, file_len) < 0) {
    free(filebuf);
    pthread_exit(NULL);
  }

  printf(" >> File name : %s File Size : %dByte\n", file_name, file_len);

  if ((packet_buf = (char*)calloc(file_len + 256, sizeof(char))) == NULL) {
    free(filebuf);
    pthread_exit(NULL);
  }

  sprintf(packet_buf, "%s%c", file_name, FILE_DELIMITER);
  memcpy(packet_buf + strlen(file_name) + sizeof(FILE_DELIMITER), filebuf, file_len);

  packet_len = strlen(file_name) + sizeof(FILE_DELIMITER) + file_len;

  printf(" >> transferring packet length : %dByte\n", packet_len);

  if (pthread_mutex_init(&client_count_lock, NULL) != 0) {
    printf("Mutex initialization failed.\n");
    pthread_exit(NULL);
  }

  if (client_count == MAX_CLIENTS) {
    for (i = 0; i < MAX_CLIENTS; ++i) {
      if (!client_sockets[i]) continue;

      send_peer_list(client_sockets[i]);
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
      pthread_mutex_lock(&client_count_lock);
      client_socket = client_sockets[i];
      pthread_mutex_unlock(&client_count_lock);

      if (!client_socket) continue;
      if (send_file(client_socket, PACKET_TYPE, packet_buf, packet_len) != 0) {
        puts("send_packet Error\n");
        free(filebuf);
        free(packet_buf);
        close(client_socket);
        pthread_mutex_lock(&client_count_lock);
        client_sockets[i] = 0;
        client_count--;
        pthread_mutex_unlock(&client_count_lock);
      } 
      if (recv(client_socket, (char*)&len, 4, 0) < 0) {
        printf("Error receiving acknowledgment from client (Socket number: %d).\n", client_socket);
        close(client_socket);
        pthread_mutex_lock(&client_count_lock);
        client_sockets[i] = 0;
        client_count--;
        pthread_mutex_unlock(&client_count_lock);
      } else {
        printf("File sent successfully to client (Socket number: %d).\n", client_socket);
        close(client_socket);
        pthread_mutex_lock(&client_count_lock);
        client_sockets[i] = 0;
        client_count--;
        pthread_mutex_unlock(&client_count_lock);
      }
      
    }

    pthread_mutex_lock(&client_count_lock);
    client_conn_count = 0;
    client_count = 0;
    pthread_mutex_unlock(&client_count_lock);
  } else {
    pthread_mutex_unlock(&client_count_lock);
    pthread_exit(NULL);
  }

  pthread_mutex_destroy(&client_count_lock);
  pthread_exit(NULL);

  return NULL;
}
int get_client_ip(int socket) {
  struct sockaddr_in sock;
  socklen_t len;

  len = sizeof(sock);
  if (getpeername(socket, (struct sockaddr *)&sock, &len) < 0)
    return -1;

  printf(" >> Connected Client IP Address : [%s]\n", inet_ntoa(sock.sin_addr));
  return 0;
}

int init_fileserver(int port_num) {
  struct sockaddr_in server_addr, client_addr;
  int server_socket, client_socket;
  socklen_t len;
  int client_port = 0;

  int connected_clients = 0;
  if ((server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    puts("socket Error!");
    return -1;
  }

  memset((char *)&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons((unsigned short)port_num);

  while (bind(server_socket, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0) {
    puts("bind Error!");
    sleep(120);
  }

  if (listen(server_socket, SOMAXCONN) < 0) {
    puts("listen Error!");
    return -1;
  }

  while(1) {
    len = sizeof(client_addr);
    if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr,
                                &len)) < 0)
      continue;
    connected_clients++;

    pthread_mutex_lock(&client_count_lock);
    if (client_count < MAX_CLIENTS) {
      client_sockets[client_count++] = client_socket;

      printf("clients Count : %d \n", client_count);
      printf("Client connected (IP: %s, Socket number: %d).\n",
                   inet_ntoa(client_addr.sin_addr), client_socket);
      client_conn_count++;
      struct sockaddr_in addr;
      socklen_t addrlen = sizeof(addr);
      getpeername(client_socket, (struct sockaddr*)&addr, &addrlen);
      char peer_address[MAX_PEER_ADDRESS];
      inet_ntop(AF_INET, &(addr.sin_addr), peer_address, MAX_PEER_ADDRESS);
      client_port = ntohs(addr.sin_port);
      printf("Client %d connected (IP: %s, Socket number: %d, Port number : %d).\n", client_conn_count, peer_address, client_socket, client_port);

      add_peer(client_conn_count, peer_address, client_port);
      pthread_t thread;
      int thread_error = pthread_create(&thread, NULL, fileserver_main,
                                        (void *)(intptr_t)client_socket);

      if (thread_error) {
        puts(" >> pthread_create Error");
        close(client_socket);
      } 
      else {
        printf(" >> Complete Thread create(socket=%d)\n", client_socket);
        get_client_ip(client_socket);
      }
    }
    else {
      pthread_mutex_unlock(&client_count_lock);
      while(client_count >= MAX_CLIENTS){
        sleep(1);
      }
    }
    pthread_mutex_unlock(&client_count_lock);
  }
    

  return 0;
}



void *do_file_service(void *params) {
  init_fileserver(server_port);

  pthread_exit(NULL);
  return NULL;
}

int main(int argc, char *argv[]) {
  pthread_t mainthread;

  if (argc != 3) {
    puts("Instructions : fileserver [Port][Filename]\n");
    puts("     ex) fileserver 3131 file.png\n\n");
    exit(0);
  }

  server_port = atoi(argv[1]);
  file_name = argv[2];
  pthread_mutex_init(&client_count_lock, NULL);
  int thread_error = pthread_create(&mainthread, NULL, do_file_service, NULL);

  if (thread_error) {
    puts(" >> Server init fail!");
  } else {
    printf(" >> Server init Complete!\n");
    pthread_join(mainthread, NULL);
  }

  return 0;
}
