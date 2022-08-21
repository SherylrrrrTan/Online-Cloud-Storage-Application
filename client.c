#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <dirent.h>
#include "Md5.c"
#define PORT 9999
#define SIZE 128

int append = 0;

void printRemoteFileInfo(int sync, int lock, int size)
{
         printf("- Remote Directory:\n");
            printf("-- File Size: %d bytes.\n", size);
            if(sync)
              printf("-- Sync Status: synced.\n");
            else
               printf("-- Sync Status: unsynced.\n");

            if(lock != 0)
              printf("-- Lock Status: locked.\n");
            else
              printf("-- Lock Status: unlocked.\n");
}
void parse(char* str, int connfd)
{
  //printf("are we in appending mode: %d \n", append);
  char str1[SIZE];
  bzero(str1, SIZE);
  strcpy(str1,str);
  
  char* tk;
  tk = strtok(str, " \n\t\r");
  
  //printf("tk: %s \n", tk);
  if(append == 1)
  {
    if(strcmp(tk, "pause") == 0)
    {
      tk = strtok(NULL, " \n\t\r");
      int time = atoi(tk);
      sleep(time);       
    }else if(strcmp(tk, "close") == 0){
      append = 0;
      int fileSize = -1;
      send(connfd, &fileSize, 4, 0);
    }else{

      int strLen = strlen(str1);
      //printf("str1: %s \n", str1);
      send(connfd, &strLen, 4, 0);
      send(connfd, str1, strLen, 0);   
      
    }
  }else{
    if(strcmp(tk, "pause") == 0)
    {
      tk = strtok(NULL, " \n\t\r");
      int time = atoi(tk);
      sleep(time); 
    }

    else if(strcmp(tk, "append") == 0)
    {
      char commd[3] = "ap";
      send(connfd, commd, strlen(commd), 0);

      tk = strtok(NULL, " \n\t\r"); //filename;
      int fileNameLength = strlen(tk);
      send(connfd, &fileNameLength, 4, 0);
      send(connfd, tk, fileNameLength, 0);

      char buff[2];
      bzero(buff,2);

      recv(connfd,buff,1,0);
    
      if(strcmp(buff,"0") == 0)
      {
          printf("File [%s] could not be found in remote directory.\n", tk);
          return;
      }else{
        append = 1;
      }
    
    }
    else if (strcmp(tk, "upload") == 0)
    {

      tk = strtok(NULL, " \n\t\r"); //filename

      char filename[SIZE] = "Local Directory/";
      strcat(filename, tk);
  

      FILE *stream;

      if ((stream = fopen(filename, "rb+")) == NULL) {
          printf("File [%s] could not be found in local directory. \n", tk);
          return;
      }
      char commd[3] = "up";
      send(connfd, commd, 2, 0);//sending commands;
      int fileNameLength = strlen(tk);
      send(connfd, &fileNameLength, 4, 0);
      send(connfd, tk, strlen(tk), 0);      
      
      int chunk_size = SIZE;
      char file_chunk[chunk_size];
      fseek(stream, 0L, SEEK_END);  // Sets the pointer at the end of the file.
      int file_size = ftell(stream);  // Get file size.
      fseek(stream, 0L, SEEK_SET);  // Sets the pointer back to the beginning of the file.

      send(connfd, &file_size, 4, 0);

      int nbyte, retval;
      //printf("Server: file size = %i bytes\n", file_size);
      
      int total_bytes = 0;  // Keep track of how many bytes we read so far.
      int current_chunk_size;  // Keep track of how many bytes we were able to read from file(helpful for the last chunk).
      ssize_t sent_bytes;
      while (total_bytes < file_size){
          // Clean the memory of previous bytes.
          bzero(file_chunk, chunk_size);

          // Read file bytes from file.
          current_chunk_size = fread(&file_chunk, sizeof(char), chunk_size, stream);
          // Sending a chunk of file to the socket.
          sent_bytes = send(connfd, &file_chunk, current_chunk_size, 0);
       
          total_bytes = total_bytes + sent_bytes;

      }
      printf("%d bytes uploaded successfully. \n", file_size); 
      if ((retval = fclose(stream)) != 0) {
          perror("upload close");
          return;
      }

    }
    else if (strcmp(tk, "download") == 0)
    {
      char commd[3] = "dl";
      send(connfd, commd, 2, 0);


      tk = strtok(NULL, " \n\t\r"); //filename
      int fileNameLength = strlen(tk);
      send(connfd, &fileNameLength, 4, 0);
      send(connfd, tk, fileNameLength, 0);

      int errorCheck;
      int nbytes = recv(connfd, &errorCheck, 4, 0);
      
      //printf("nbytes: %d \n", nbytes);
      //printf("errorcheck: %d \n", errorCheck);
      if(errorCheck == 2)
      {
        printf("File [%s] could not be found in remote directory. \n", tk);
        return;
      }
      if(errorCheck == 1)
      {
        printf("File [%s] is currently locked by another user.\n", tk);
        return;
      }

      FILE *stream;
      char filename[SIZE] = "Local Directory/";
      strcat(filename, tk);
      //printf("full filename: %s \n", filename);
      
      if ((stream = fopen(filename, "wb+")) == NULL) {
          perror("download open");
          return;
      }

      int received_bytes;
      int fileSize;
      recv(connfd, &fileSize, 4, 0);
      int total_bytes = 0;

      int chunk_size = SIZE;
      char temp[chunk_size];
      bzero(temp, chunk_size);

      while(1){
        bzero(temp, chunk_size);
        int remaining_bytes = fileSize - total_bytes;
        if(remaining_bytes <= chunk_size)
        {
          received_bytes = recv(connfd, temp, remaining_bytes, 0);
          fwrite(&temp, sizeof(char), received_bytes, stream);
          break;
        }
        received_bytes = recv(connfd, temp, chunk_size, 0);
        total_bytes = total_bytes + received_bytes;
        fwrite(&temp, sizeof(char), received_bytes, stream);
      }

      printf("%d bytes downloaded successfully. \n", fileSize);

      int retval;
      if ((retval = fclose(stream)) != 0) {
          perror("upload close");
          return;
      }
    
      
    }
    else if (strcmp(tk, "syncheck") == 0)
    {
      
      char commd[3] = "sy";
      send(connfd, commd, strlen(commd), 0);

      tk = strtok(NULL, " \n\t\r"); //filename

      int fileNameLength = strlen(tk);
      send(connfd, &fileNameLength, 4, 0);
      send(connfd, tk, fileNameLength , 0);  

      char path[SIZE] = "Local Directory/";
      strcat(path, tk);
        //strcat(path, "\0");
      
      printf("Sync Check Report:\n");


      int lock;
      recv(connfd, &lock, 4, 0);
      char onRemote[2];
      bzero(onRemote,2);
      
      if(access(path, F_OK) == 0)
      {
        char* Message = "1";
        send(connfd, Message, strlen(Message), 0); 
        recv(connfd,onRemote,1,0);
          
          /* to print info about local file*/
           
          FILE *fp = fopen(path,"r");
          fseek(fp, 0, SEEK_END); //in client 
          int sz = ftell(fp);
          rewind(fp);
          
          printf("- Local Directory:\n");
          printf("-- File Size: %d bytes.\n", sz);

            /* to print info about local file*/

          if(strcmp(onRemote,"1") == 0)
          {
                                   
            char content1[33]; 
            memset(content1, '\0', 33);
            
            MDFile(path, content1);
            //printf("path md5 hashcode: %s \n", content1);
            
            
            send(connfd, content1, 32, 0);

            int sync, size;
            recv(connfd, &size, 4, 0);
            recv(connfd, &sync, 4, 0);
            //recv(connfd, &lock, 4, 0);
            printRemoteFileInfo(sync, lock, size);
          }
          
        }
        else
        {
          char* Message = "0";
          send(connfd, Message, strlen(Message), 0);
          recv(connfd,onRemote,1,0);
        
          if(strcmp(onRemote,"1") == 0)
          {
            int sync, lock, size;
            recv(connfd, &size, 4, 0);
            recv(connfd, &sync, 4, 0);
            recv(connfd, &lock, 4, 0);
            printRemoteFileInfo(sync, lock, size);
          }
        }
    }
    else if (strcmp(tk, "delete") == 0)
    {
        char commd[3] = "de";
        send(connfd, commd, strlen(commd), 0);

        tk = strtok(NULL, " \n\t\r"); //filename

        int fileNameLength = strlen(tk);
        send(connfd, &fileNameLength, 4, 0);
        send(connfd, tk, fileNameLength , 0);   
      
        char buff[2];
        bzero(buff,2);
        recv(connfd,buff,1,0);
        if(strcmp(buff,"1") == 0)
        {
            printf("File deleted successfully. \n");
        }else{
            printf("File [%s] could not be found in remote directory.\n", tk);
        } 
    }
    else if(strncmp(tk, "quit", 4) == 0)
    {
        close(connfd);
    }else{
        printf("Command [%s] is not recognized. \n", str1);
    }
  }
}

int main(int argc, char **argv) {
    int client_socket;
    append = 0;
    struct sockaddr_in serv_addr;
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (client_socket < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    // The server IP address should be supplied as an argument when running the 
    //application.
    int addr_status = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr);
    if (addr_status <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    int connect_status = connect(client_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (connect_status < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

  char buf[1024];
  bzero(buf, 1024);
  FILE *stream;
  int nbyte, retval;
 // int clientfd = open_clientfd(argv[2], "9999");
  
  if ((stream = fopen(argv[1], "rb")) == NULL) {
    perror("open");
    exit(1);
  }

  if((nbyte = fread(buf, sizeof(char), 1024, stream)) < 0)
  {
    perror("read");
    exit(1);
  }
  //printf("how many bytes read: %d \n", nbyte);
  if ((retval = fclose(stream)) != 0) {
    perror("close");
    exit(1);
  }

  char* temp;
  char* token;
  token = strtok_r(buf, "\n", &temp);
  printf("Welcome to ICS53 Online Cloud Storage.\n");

  while(token != NULL)
  {
    if(append == 0) printf("> %s \n", token);
    else printf("Appending> %s \n", token);
    parse(token, client_socket);
    token = strtok_r(NULL, "\n", &temp);
  }
  
  return 0;  
}
