#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>




#define EOL "\r\n"
#define EOL_SIZE 2

typedef struct
{
    char *ext;
    char *mediatype;
} extn;

extn extensions[] =
{
    {"gif", "image/gif" },
    {"txt", "text/plain" },
    {"jpg", "image/jpg" },
    {"jpeg", "image/jpeg"},
    {"png", "image/png" },
    {"ico", "image/ico" },
    {"zip", "image/zip" },
    {"gz",  "image/gz"  },
    {"tar", "image/tar" },
    {"htm", "text/html" },
    {"html", "text/html" },
    {"php", "text/html" },
    {"pdf", "application/pdf"},
    {"zip", "application/octet-stream"},
    {"rar", "application/octet-stream"},
    {"py", "text/html"},
    {0, 0}
};


void error(const char *msg)
{
    perror(msg);
    exit(1);
}



void send_new(int fd, char *msg)
{
    int len = strlen(msg);
    printf("send:%s\n", msg);
    if (send(fd, msg, len, 0) == -1)
    {
        printf("Error in send\n");
    }
}



int recv_new(int fd, char *buffer)
{
    char *p = buffer; // Use of a pointer to the buffer rather than dealing with the buffer directly
    int eol_matched = 0; // Use to check whether the recieved byte is matched with the buffer byte or not
    while (recv(fd, p, 1, 0) != 0) // Start receiving 1 byte at a time
    {
        if (*p == EOL[eol_matched]) // if the byte matches with the first eol byte that is '\r'
        {
            ++eol_matched;
            if (eol_matched == EOL_SIZE) // if both the bytes matches with the EOL
            {
                *(p + 1 - EOL_SIZE) = '\0'; // End the string
                return (strlen(buffer)); // Return the bytes recieved
            }
        }
        else
        {
            eol_matched = 0;
        }
        p++; // Increment the pointer to receive next byte
    }
    return (0);
}

char *webroot()
{
    // open the file "conf" for reading
    FILE *in = fopen("conf", "rt");
    // read the first line from the file
    char buff[1000];
    fgets(buff, 1000, in);
    // close the stream
    fclose(in);
    char *nl_ptr = strrchr(buff, '\n');
    if (nl_ptr != NULL)
        *nl_ptr = '\0';
    return strdup(buff);
}

int get_file_size(int fd)
{
    struct stat stat_struct;
    if (fstat(fd, &stat_struct) == -1)
        return (1);
    return (int) stat_struct.st_size;
}

void python_cgi(char *script_path, int fd)
{
    //printf ("Contenttype:text/plain\r\n\r\n");

    send_new(fd, "HTTP/1.1 200 OK\n Server: Web Server in C\n Connection: close\n");
    dup2(fd, STDOUT_FILENO);
    char script[500];
    strcpy(script, "SCRIPT_FILENAME=");
    strcat(script, script_path);
    putenv("GATEWAY_INTERFACE=CGI/1.1");
    putenv(script);
    putenv("QUERY_STRING=");
    putenv("REQUEST_METHOD=GET");
    putenv("REDIRECT_STATUS=true");
    putenv("SERVER_PROTOCOL=HTTP/1.1");
    putenv("REMOTE_HOST=127.0.0.1");

    execl("/usr/bin/python3", "python3", script_path, NULL);
    //send_new(fd,result);
}

int connection(int fd)
{

    char request[BUFSIZ], resource[500], *ptr;

    int fd1, length;

    if (recv(fd, request, BUFSIZ, 0) == 0)
    {
        printf("Recieve Failed\n");
    }

    printf("%s\n", request);
    // Check for a valid browser request

    ptr = strstr(request, " HTTP/");

    if (ptr == NULL)
    {
        printf("NOT HTTP !\n");
    }
    else
    {
        *ptr = 0;
        ptr = NULL;

        if (strncmp(request, "GET ", 4) == 0)
        {
            printf("%s\n", request);
            ptr = request + 4;
        }

        if (ptr == NULL)
        {
            printf("Unknown Request ! \n");
        }
        else
        {
            printf("ptr:%s\n", ptr);

            if (ptr[strlen(ptr) - 1] == '/')
            {
                strcat(ptr, "index.html");
            }
            strcpy(resource, webroot());
            strcat(resource, ptr);
            char *s = strchr(ptr, '.');
            int i;
            for (i = 0; extensions[i].ext != NULL; i++)
            {
                if (strcmp(s + 1, extensions[i].ext) == 0)
                {
                    fd1 = open(resource, O_RDONLY, 0);
                    printf("Opening \"%s\"\n", resource);
                    if (fd1 == -1)
                    {
                        printf("404 File not found Error\n");
                        send_new(fd, "HTTP/1.1 404 Not Found\r\n");
                        send_new(fd, "Server : Web Server in C\r\n\r\n");
                        send_new(fd, "<html><head><title>404 Not Found</head></title>");
                        send_new(fd, "<body><p>404 Not Found: The requested resource could not be found!</p></body></html>");
                        //Handling php requests
                    }
                    else if (strcmp(extensions[i].ext, "py") == 0)
                    {
                        python_cgi(resource, fd);
                        sleep(1);
                        close(fd);
                        exit(1);
                    }
                    else
                    {
                        printf("200 OK, Content-Type: %s\n\n", extensions[i].mediatype);

                        send_new(fd, "HTTP/1.1 200 OK\r\n");
                        send_new(fd, "Server : Web Server in C\r\n\r\n");

                        if (ptr == request + 4) // if it is a GET request
                        {
                            if ((length = get_file_size(fd1)) == -1)
                                printf("Error in getting size !\n");

                            size_t total_bytes_sent = 0;
                            ssize_t bytes_sent;

                            while (total_bytes_sent < length)
                            {
                                //Zero copy optimization
                                if ((bytes_sent = sendfile(fd, fd1, 0, length - total_bytes_sent)) <= 0)
                                {
                                    if (errno == EINTR || errno == EAGAIN)
                                    {
                                        continue;
                                    }
                                    perror("sendfile");
                                    return -1;
                                }

                                total_bytes_sent += bytes_sent;
                            }

                        }
                    }
                    break;
                }
                int size = sizeof(extensions) / sizeof(extensions[0]);
                if (i == size - 2)
                {
                    printf("415 Unsupported Media Type\n");
                    send_new(fd, "HTTP/1.1 415 Unsupported Media Type\r\n");
                    send_new(fd, "Server : Web Server in C\r\n\r\n");
                    send_new(fd, "<html><head><title>415 Unsupported Media Type</head></title>");
                    send_new(fd, "<body><p>415 Unsupported Media Type!</p></body></html>");
                }
            }

            close(fd);
        }
    }
    shutdown(fd, SHUT_RDWR);
}

void html_handle(int fd)
{
    char req[BUFSIZ];
    char request[500], resource[500], *ptr;

    int fd1, length;

    if (recv(fd, req, BUFSIZ, 0) == 0)
    {
        printf("Recieve Failed\n");
    }

    printf("%s\n", request);

    send_new(fd, "HTTP/1.0 200 OK\r\n\r\n");
    send_new(fd, "<!DOCTYPE html><html><head></head><body><h1>HTTP Server</h1></body></html>");


}

int main(int argc, char *argv[])
{
    int server_sockfd;//服务器端套接字
    int client_sockfd;//客户端套接字
    int len, pid;
    struct sockaddr_in my_addr;   //服务器网络地址结构体
    struct sockaddr_in remote_addr; //客户端网络地址结构体
    int sin_size;
    char buf[BUFSIZ];  //数据传送的缓冲区
    memset(&my_addr, 0, sizeof(my_addr)); //数据初始化--清零
    my_addr.sin_family = AF_INET; //设置为IP通信
    my_addr.sin_addr.s_addr = INADDR_ANY; //服务器IP地址--允许连接到所有本地地址上
    my_addr.sin_port = htons(8000); //服务器端口号

    /*创建服务器端套接字--IPv4协议，面向连接通信，TCP协议*/
    if((server_sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return 1;
    }

    /*将套接字绑定到服务器的网络地址上*/
    if (bind(server_sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0)
    {
        perror("bind");
        return 1;
    }

    /*监听连接请求--监听队列长度为5*/
    listen(server_sockfd, 5);

    sin_size = sizeof(struct sockaddr_in);

    /*等待客户端连接请求到达*/

    while (1)
    {
        client_sockfd = accept(server_sockfd, (struct sockaddr *) &remote_addr, &sin_size);
        // send(client_sockfd, "Welcome to my server\n", 21, 0);
        if (client_sockfd < 0)
            error("ERROR on accept");
        pid = fork();
        if (pid < 0)
            error("ERROR on fork");
        if (pid == 0)
        {
            close(server_sockfd);
            // char html[] = u8"HTTP/1.0 200 OK\r\nDate:Thu, Otc 11 2016 15:03:08 GMT\r\nServer:HTTPServer\r\nConnection:keep-alive\r\nContent-Type:text/html;charset=utf-8\r\nContent-Length:74\r\n \r\n<!DOCTYPE html><html><head></head><body><h1>HTTP Server</h1></body></html>";
            // recv(client_sockfd, buf, BUFSIZ, 0);
            // printf("%s\n", buf);
            // send(client_sockfd, html, strlen(html), 0);
            // send_new(client_sockfd,"HTTP/1.0 200 OK\r\n\r\n");
            // send_new(client_sockfd,"<!DOCTYPE html><html><head></head><body><h1>HTTP Server</h1></body></html>");

            connection(client_sockfd);
            // html_handle(client_sockfd);
            close(client_sockfd);
            exit(0);
        }
        else
            close(client_sockfd);
    } /* end of while */
    close(server_sockfd);
    return 0;
}




