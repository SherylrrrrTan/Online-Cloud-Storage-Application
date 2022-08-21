#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include "Md5.c"
#include <pthread.h>
#define PORT 9999
#define SIZE 128

pthread_mutex_t lock;

void parse(char* buf, int connfd)
{
    
    if(strcmp(buf, "up") == 0){ //upload
        int retval;

        int fileNameSize;
        recv(connfd, &fileNameSize, 4,0);
         
        char fileName[fileNameSize+1];
        bzero(fileName,fileNameSize+1);
        recv(connfd, &fileName, fileNameSize, 0);
        fileName[fileNameSize] = '\0';



        char filename[SIZE] = "Remote Directory/";
        strcat(filename, fileName);
        
        //printf("upload filename : %s \n", filename);

        FILE *stream;
        if((stream = fopen(filename, "wb+")) == NULL)
        {
            perror("upload read");
            return;
        }

        int file_size;
        recv(connfd, &file_size, 4, 0);
        int chunk_size = SIZE;
        char fileContent[chunk_size];
        
        int received_bytes;
        int total_bytes = 0;
        
        while(1)
        {
          bzero(fileContent, chunk_size);
          int remaining_bytes = file_size - total_bytes;
          
          if(remaining_bytes <= chunk_size)
          {
            received_bytes = recv(connfd, &fileContent, remaining_bytes, 0);
              
            fwrite(&fileContent, sizeof(char), received_bytes, stream);
            break;
          }
          received_bytes = recv(connfd, &fileContent, chunk_size, 0);
          total_bytes = total_bytes + received_bytes;
          fwrite(&fileContent, sizeof(char), received_bytes, stream);
        }
          if ((retval = fclose(stream)) != 0) {
              perror("upload close");
        }              
      }
      else if(strcmp(buf, "ap") == 0) //append
      {
        FILE * stream;
        int fileNameSize;
        recv(connfd, &fileNameSize, 4, 0);

        char filename2[fileNameSize+1];
        bzero(filename2, fileNameSize+1);
        recv(connfd, filename2, fileNameSize, 0);
        filename2[fileNameSize] = '\0';
        char filename[SIZE] = "Remote Directory/";
        strcat(filename, filename2);

        if((stream = fopen(filename, "r")) == NULL)
        {     
            char* errMessage = "0";
            send(connfd, errMessage, strlen(errMessage), 0);
            return;
        }

        int retval;
        if ((retval = fclose(stream)) != 0) {
            perror("append close");
            return;
        }   

        char* Message = "1";
        send(connfd, Message, strlen(Message), 0);

        pthread_mutex_lock(&lock);
        stream = fopen(filename, "ab+");

        int file_size;
        recv(connfd, &file_size, 4, 0);

        while(file_size != -1)
        {
          char writtenToFile[file_size+1];
          memset(writtenToFile, '\0', file_size+1);
          recv(connfd, writtenToFile, file_size, 0);
          char* newLine = "\n";
          fwrite(newLine, sizeof(char), strlen(newLine), stream);
          int nbyte = fwrite(&writtenToFile, sizeof(char), strlen(writtenToFile), stream);
          recv(connfd, &file_size, 4, 0);
        }
       
        if ((retval = fclose(stream)) != 0) {
            perror("append close");
        } 
        pthread_mutex_unlock(&lock);
        return;
      }
      else if(strcmp(buf, "dl") == 0) //download
      {
        int fileNameSize;
        recv(connfd, &fileNameSize, 4, 0);

        char filename2[fileNameSize+1];
        bzero(filename2, fileNameSize+1);
        recv(connfd, filename2, fileNameSize, 0);
        filename2[fileNameSize] = '\0';
        
        char filename[SIZE] = "Remote Directory/";
        strcat(filename, filename2);

        int lockCheck;
        if(access(filename, F_OK) != 0){
          lockCheck = 2;
          send(connfd, &lockCheck, 4, 0); // not exists
          return;
        }
        lockCheck = pthread_mutex_trylock(&lock);
        if(lockCheck == 0)
        {
          send(connfd, &lockCheck, 4, 0); // not occupied
          pthread_mutex_unlock(&lock);
        }else{
          lockCheck = 1;
          send(connfd, &lockCheck, 4, 0); //occupied
          return;
        }


        //printf("full file name: %s \n", filename);
          
        FILE* stream;
        if((stream = fopen(filename, "rb")) == NULL) return;

        fseek(stream, 0L, SEEK_END);  // Sets the pointer at the end of the file.
        int file_size = ftell(stream);  // Get file size.
        fseek(stream, 0L, SEEK_SET);

        send(connfd, &file_size, 4, 0);

          //printf("file_size: %d \n", file_size);
        int total_bytes = 0;  // Keep track of how many bytes we read so far.
        int current_chunk_size;  // Keep track of how many bytes we were able to read from file(helpful for the last chunk).
        ssize_t sent_bytes;

        int chunk_size = SIZE;
        char file_chunk[chunk_size];
        while (total_bytes < file_size){
                // Clean the memory of previous bytes.
            bzero(file_chunk, chunk_size);

                // Read file bytes from file.
            current_chunk_size = fread(&file_chunk, sizeof(char), chunk_size, stream);
                // Sending a chunk of file to the socket.
            sent_bytes = send(connfd, &file_chunk, current_chunk_size, 0);
                //printf("file_chunk : %s \n", file_chunk);
            total_bytes = total_bytes + sent_bytes;
           
        }
        int retval;
        if ((retval = fclose(stream)) != 0) {
          perror("upload close");
          return;
        }
      
      }
      else if(strcmp(buf, "de") == 0)//delete
      {
        int fileNameSize;
        recv(connfd, &fileNameSize, 4, 0);

        char fileName[fileNameSize+1];
        recv(connfd, fileName, fileNameSize, 0);
        fileName[fileNameSize] = '\0';


        
        char path[SIZE] = "Remote Directory/";
        strcat(path, fileName);

        if (remove(path) == 0) {
          char* Message = "1";
          send(connfd, Message, strlen(Message), 0);
          //printf("The file is deleted successfully. \n");
        }
        else {
            char* errMessage = "0";
            send(connfd, errMessage, strlen(errMessage), 0);
        }

  
      }
      else if(strcmp(buf, "sy") == 0)
      {
        int fileNameSize;
        recv(connfd, &fileNameSize, 4, 0);
        char fileName[fileNameSize+1];
        memset(fileName, '\0', fileNameSize+1);
        recv(connfd, fileName, fileNameSize, 0);
        char path[SIZE] = "Remote Directory/";
        strcat(path, fileName);

        int lock_status = pthread_mutex_trylock(&lock);
        send(connfd, &lock_status, 4, 0);
        //printf("lock_status = %i\n", lock_status);
        while (lock_status !=0){  
          usleep(100000);  // 0.1s
          lock_status = pthread_mutex_trylock(&lock);
        }
        pthread_mutex_unlock(&lock);
       
        
        char onLocal[2];
        bzero(onLocal,2);
        recv(connfd,onLocal,1,0);
        
        if(access(path, F_OK) == 0)
        {

          char* Message = "1"; //on remote
          send(connfd, Message, strlen(Message), 0);
      
          /* to print info about server file*/
          FILE *fp = fopen(path,"r");

          fseek(fp, 0L, SEEK_END);  // Sets the pointer at the end of the file.
          int sz = ftell(fp);  // Get file size.
          fseek(fp, 0L, SEEK_SET);
          /* to print info about server file*/

          int sync = 0;
          
          
          if(strcmp(onLocal,"1") == 0)
          {
            char chash[33];
            memset(chash, '\0', 33); 
               
            char content1[33];      
            memset(content1, '\0', 33);   
            
                       
            MDFile(path, content1);
            //printf("content1: %s \n", content1);
            

            recv(connfd, chash, 32, 0);
            //printf("chash: %s \n", chash);
            if(strncmp(content1, chash, 32) == 0)
            {
              //printf("hashcodes match \n");
              sync = 1;
            }
          }

          send(connfd, &sz, 4, 0);
          send(connfd, &sync, 4, 0);
          //send(connfd, &onlock, 4, 0);
          
        }
        else //file not on Remote
        {
          char* Message = "0";
          send(connfd, Message, strlen(Message), 0);
        }

      }

}

void echo(int connfd)
{
    //printf("start echo \n");
  pthread_detach(pthread_self());
  size_t n;
  char buf[3];
  bzero(buf,3);
  while((n = recv(connfd, buf, 2, 0)) > 0) { 
    //printf("now parsing \n");
    parse(buf, connfd);
    bzero(buf,3);   
  }

}


int main(int argc, char **argv)
{
  char client_hostname[SIZE], client_port[SIZE];
  int client_socket, server_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);
  pthread_t tid;

  pthread_mutex_init(&lock, NULL);
  

  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket == 0) {
      perror("socket failed");
      exit(EXIT_FAILURE);
  }
  int socket_status = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR , &opt, sizeof(opt));
  if (socket_status) {
      perror("setsockopt");
      exit(EXIT_FAILURE);
  }
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
    
  address.sin_port = htons(PORT);
  int bind_status = bind(server_socket, (struct sockaddr*)&address, sizeof(address));
  if (bind_status < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
  }
  int listen_status = listen(server_socket, 5);
  if (listen_status < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
  }
  
  while (1) {
    client_socket = accept(server_socket, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    if (client_socket < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
 
    pthread_create(&tid, NULL, echo, client_socket);
  }
  pthread_mutex_destroy(&lock);
  return 0;
}
