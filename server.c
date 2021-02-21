#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_ON_WELCOME 5
#define BUFLEN 4000
#define RESPONSE 512
#define ERROR_HTML 256
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define STATUS 256
#define ERROR_302 302
#define DIR_ENTITY 500

char *get_mime_type(char *name)
{
char *ext = strrchr(name, '.');
if (!ext) return NULL;
if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
if (strcmp(ext, ".gif") == 0) return "image/gif";
if (strcmp(ext, ".png") == 0) return "image/png";
if (strcmp(ext, ".css") == 0) return "text/css";
if (strcmp(ext, ".au") == 0) return "audio/basic";
if (strcmp(ext, ".wav") == 0) return "audio/wav";
if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
return NULL;
}


int handle_client(void* x)
{
    int fd=*(int*)x;
    int isError=0;
    int isFile=0;
    int status=0;
    char* phrase=NULL;
    char* path=NULL;
    char* method=NULL;
    char* protocol=NULL;
    unsigned char* body=NULL;
    char* type=NULL;
    int size=0;
    int isContent=0;

    time_t now;
    char timebuf[128];
    now = time(NULL);

    // in case accept failed in main failed
    if(fd<0){
        perror("ERROR on accept");
        isError=1;
        status=500;
        phrase="Internal Server Error";
        body="Some server side error.";
        type="text/html";
    }
    int rc=0;
    char request[BUFLEN]={0};
    rc=read(fd,request,BUFLEN);
    // in case read failed
    if(rc < 0){
        perror("Read failed");
        isError=1;
        status=500;
        phrase="Internal Server Error";
        body="Some server side error.";
        type="text/html";
    }

    //get tokens
    const char s[2]=" ";
    method=strtok(request,s);
    path=strtok(NULL,s);
    protocol=strtok(NULL,s);

    if(protocol!=NULL){
        int k=0;
        while(protocol[k]!='\r')
            k++;

    protocol[k]='\0';
    }

    // in case tokens number is less than 3.   
    if(method==NULL || path==NULL || protocol==NULL){
        isError=1;
        status=400;
        phrase="Bad Request";
        body="Bad Request.";
        type="text/html";
        
    }

    // in case protocol is not HTTP/1.0 or HTTP/1.1
    else if(strncmp(protocol,"HTTP/1.0",strlen(protocol))!=0 && strncmp(protocol,"HTTP/1.1",strlen(protocol))!=0 ){
        isError=1;
        status=400;
        phrase="Bad Request";
        body="Bad Request.";
        type="text/html";
    }

   // in case method is not GET
    else if(strncmp(method,"GET",strlen(method))!=0){
        isError=1;
        status=501;
        phrase="Not supported";
        body="Method is not supported.";
        type="text/html";
    }
    // files
    struct stat ftemp;
    struct stat fs;
    int file=0;
    struct dirent* dentry;
    DIR* dir;

    if(isError==0){

        char full_path[STATUS]={0};
        full_path[0]='.';
        strcat(full_path,path);

        // only for dir content
        char full_path2[STATUS]={0};
        full_path2[0]='.';
        strcat(full_path2,path);

        dir=opendir(full_path);

        // in case path is not directory or no permissions
        if(dir==NULL){
            
            struct stat ftemp;
            char* temp = (char*)calloc(sizeof(full_path),sizeof(char));
            int i = 0;

            while(i < strlen(full_path))
            {
                while(full_path[i] != '/' && i < strlen(full_path))
                {
                    temp[i] = full_path[i];
                    i++;
                }

                if(stat(temp,&ftemp) < 0)
                {
                    isError=1;
                    status=404;
                    phrase="Not Found";
                    body="File not found.";
                    type="text/html";
                    break;
                }

                if(S_ISREG(ftemp.st_mode))
                { 
                    if((ftemp.st_mode & S_IROTH) && (ftemp.st_mode & S_IRGRP) && (ftemp.st_mode & S_IRUSR)){}
                    else
                    {
                        isError=1;
                        status=403;
                        phrase="Forbidden";
                        body="Access denied.";
                        type="text/html";
                        break;
                    }
                }
                else if(S_ISDIR(ftemp.st_mode))
                {
                    if(ftemp.st_mode & S_IXOTH ){}
                    else
                    {
                        isError=1;
                        status=403;
                        phrase="Forbidden";
                        body="Access denied.";
                        type="text/html";
                        break;
                    }
                }               

                temp[i]='/';
                i++;
            }

            free(temp);      
                              
                // return file
                if(status==0)
                {    
                    file=open(full_path,O_RDONLY);
                    stat(full_path,&fs);
                    isFile=1;
                    status=200;
                    phrase="OK";
                    type=get_mime_type(path);
                    size=(int)fs.st_size;
                    body=(unsigned char*)calloc(size+1,sizeof(unsigned char));
                    read(file,body,size);
                }
                
        }

        // directory found
        else
        {
            // 302 ERROR
            if(path[strlen(path)-1]!='/')
            {
                   isError=1;
                   status=302;
                   phrase="Found";
                   body="Directories must end with a slash.";
                   type="text/html";
                   closedir(dir);
            }

            // directory found
            else
            {    
                // index.html file        
                strcat(full_path,"index.html");
                file=open(full_path,O_RDONLY);
                if(file>0)
                {
                    stat(full_path,&fs);
                    status=200;
                    phrase="OK";
                    type="text/html";
                    isFile=1;
                    size=(int)fs.st_size;
                    body=(unsigned char*)calloc(size+1,sizeof(unsigned char));
                    read(file,body,size);
                    closedir(dir);
                }   

                // no index.html, return contents of the directory 
                else
                {
                    stat(full_path2,&fs);
                    status=200;
                    phrase="OK";
                    type="text/html";
                    isFile=1;
                    isContent=1;
                    
                    while((dentry=readdir(dir))!=NULL)
                    {
                        size+=DIR_ENTITY;
                    }
                    closedir(dir);   
                    body=(unsigned char*)calloc((size+1+RESPONSE),sizeof(unsigned char));

                    strcat(body,"<HTML>\r\n<HEAD><TITLE>Index of ");
                    strcat(body,full_path2);
                    strcat(body,"</TITLE></HEAD>\r\n\r\n<BODY>\r\n<H4>Index of ");
                    strcat(body,full_path2);
                    strcat(body,"</H4>\r\n\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n");

                    dir=opendir(full_path2);

                    while((dentry=readdir(dir))!=NULL)
                    {
                        
                        char* temp_path=(char*)calloc(strlen(full_path2)+1+strlen(dentry->d_name),sizeof(char));
                        strcat(temp_path,full_path2);
                        strcat(temp_path,dentry->d_name);

                        stat(temp_path,&ftemp);
                        strcat(body,"<tr>\r\n<td><A HREF=");
                        strcat(body,dentry->d_name);
                        strcat(body,">");
                        strcat(body,dentry->d_name);
                        strcat(body,"</A></td><td>");
                        strftime(timebuf, sizeof(timebuf), RFC1123FMT, localtime(&ftemp.st_mtime));
                        strcat(body,timebuf);
                        strcat(body,"</td>\r\n<td>");

                        if(!S_ISDIR(ftemp.st_mode))
                        {
                            int t=(int)ftemp.st_size;
                            sprintf(timebuf,"%d",t);
                            strcat(body,timebuf);
                        }

                        strcat(body,"</td>\r\n</tr>"); 

                        free(temp_path);
                    }

                    strcat(body,"\r\n\r\n</table>\r\n\r\n<HR>\r\n\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n\r\n</BODY></HTML>\r\n");
                    closedir(dir);

                }

            }


        }


    }

    char status_response[STATUS]={0};
    char html[ERROR_HTML]={0};
      
    if(isError==1)
    {
         //HTML Text    
        strcat(html,"<HTML><HEAD><TITLE>");
        sprintf(status_response,"%d",status);
        strcat(html,status_response);
        strcat(html," ");
        strcat(html,phrase);
        strcat(html,"</TITLE></HEAD>\r\n<BODY><H4>");
        strcat(html,status_response);
        strcat(html," ");
        strcat(html,phrase);
        strcat(html,"</H4>\r\n");
        strcat(html,body);
        strcat(html,"\r\n</BODY></HTML>\r\n");
    }
        //Response headers
        char response[RESPONSE]={0};
        strcat(response,"HTTP/1.1 ");
        sprintf(status_response,"%d",status);
        strcat(response,status_response);
        strcat(response," ");
        strcat(response,phrase);
        strcat(response,"\r\nServer: webserver/1.0\r\nDate: ");
        strftime(timebuf, sizeof(timebuf), RFC1123FMT, localtime(&now));
        strcat(response,timebuf);

        if(status==ERROR_302){
            strcat(response,"\r\nLocation: ");
            strcat(response,path);
            strcat(response,"/");
        }
        if(type!=NULL)
        {
            strcat(response,"\r\nContent-Type: ");
            strcat(response,type);
        }
        
        strcat(response,"\r\nContent-Length: ");
        
        if(isError==1)
        {
            sprintf(status_response,"%ld",strlen(html));
            strcat(response,status_response);
        }
        else
        {
            if(isContent==1)
                sprintf(status_response,"%ld",strlen(body));
            else    
                sprintf(status_response,"%d",size);

            strcat(response,status_response);
            strcat(response,"\r\nLast-Modified: ");
            strftime(timebuf, sizeof(timebuf), RFC1123FMT, localtime(&fs.st_mtime));
            strcat(response,timebuf);
        }


        strcat(response,"\r\nConnection: close\r\n\r\n");
        if(isError==1)
            strcat(response,html);
    
        
        write(fd,response,strlen(response));

        if(isFile==1){
            write(fd,body,size); 
            free(body);
        }

    close(fd);
}

int main(int argc,char* argv[]){


if(argc!=4)
{
    printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
    exit(EXIT_FAILURE);
}

int sockfd,newsockfd;      // socket

struct sockaddr_in serv_addr;

sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){ 
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

serv_addr.sin_family = AF_INET;
serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
serv_addr.sin_port = htons(atoi(argv[1]));

if(bind(sockfd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0){
    perror("ERROR on binding");
    exit(EXIT_FAILURE);
}

int* requests=(int*)calloc(atoi(argv[3]),sizeof(int));
if(requests==NULL){
    perror("Cannot alloacte memory");
    exit(EXIT_FAILURE);
}

threadpool* pool=create_threadpool(atoi(argv[2]));
if(pool==NULL){
    free(requests);
    exit(EXIT_FAILURE);
}

listen(sockfd,MAX_ON_WELCOME);

for(int i=0; i<atoi(argv[3]) ;i++)
{
    newsockfd=accept(sockfd,NULL,NULL);
    requests[i]=newsockfd;
    dispatch(pool,handle_client,&requests[i]);
}

close(sockfd);
destroy_threadpool(pool);
free(requests);

return EXIT_SUCCESS;
}