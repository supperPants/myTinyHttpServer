/**
 * @file httpd.c
 * @brief 一个简单的http服务器。只能用来处理静态资源，采用select模型。
 *
 * @author 史双雷
 * @data 2020年2月22日
 */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "wrap.h"

#define SERVERPORT 50000
#define BUFFSIZE 1024
#define SERVER_STRING "Server:jdbhttpd/0.1.0\r\n"

struct httpReq{
	char method[9];
	char url[255];
};
struct httpRes{
	char statCode[4];
	char statWord[255];
};

int startup();
void readRequest(int, struct httpReq*);
void unimplemented(int, const struct httpRes*);
void notFound(int, const struct httpRes*);
void serveFile(int, const struct httpRes*, const char*);
void sendline(int, const char*);
void sendStatRow(int, const struct httpRes*);
void sendResHead(int);
int isExist(const char *);
void fclear(int);
int readLine(int, char*, int);

int main(){
	//1.创建监听套接字
	int serverfd = startup();
	printf("服务器已经启动\n");
	//2.用select模型监听套接字
	fd_set rset, allset;
	int client[FD_SETSIZE];

	int i;
	for(i = 0; i < FD_SETSIZE; i++){
		client[i] = -1;
	}

	FD_ZERO(&allset);
	FD_SET(serverfd, &allset);
	
	int nready, maxfd, maxi;

	maxfd = serverfd;
	maxi = -1;
	
	int sockfd, connfd;
	struct sockaddr_in cliaddr;
	socklen_t clilen;

	struct httpReq req;
	struct httpRes res;
	char path[255];
	char str[20];

	while(1){
		rset = allset;

		nready = select(maxfd+1, &rset, NULL, NULL, NULL);
		if(nready == -1){
			perr_exit("select");
		}

		if(FD_ISSET(serverfd, &rset)){
			clilen = sizeof(cliaddr);
			connfd = Accept(serverfd, (struct sockaddr *)&cliaddr, &clilen);

			printf("from %s connection, port %d, connfd:%d\n", inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)), ntohs(cliaddr.sin_port), connfd);

			
			for(i = 0; i < FD_SETSIZE; i++){
				if(client[i] < 0){
					client[i] = connfd;
					break;
				}
			}
			if(i == FD_SETSIZE){
				fputs("客户端连接过多\n", stderr);
				exit(1);
			}

			FD_SET(connfd, &allset);

			maxfd = (maxfd < connfd ? connfd : maxfd);
			maxi = (maxi < i ? i : maxi);
			
			if((--nready) == 0)
				continue;
		}
		
		for(i = 0; i <= maxi; i++){
			if((sockfd = client[i]) < 0)
				continue;

			if(FD_ISSET(sockfd, &rset)){
				printf("sockfd:%d\n", sockfd);
				readRequest(sockfd, &req);
				if(strcmp(req.method, "GET") != 0){
					strcpy(res.statCode, "501");
					strcpy(res.statWord, "Method Not Implemented");
					unimplemented(sockfd, &res);
				}
				else{
					sprintf(path, "./webapp%s", req.url);

					if(path[strlen(path)-1] == '/'){
						strcat(path, "index.html");
					}

					if(isExist(path)){
						strcpy(res.statCode, "200");
						strcpy(res.statWord, "OK");
						serveFile(sockfd, &res, path);
					}
					else{
						strcpy(res.statCode, "404");
						strcpy(res.statWord, "NOT FOUND");
						notFound(sockfd, &res);
					}
				}
				FD_CLR(sockfd, &allset);
				client[i] = -1;
				close(sockfd);

				if(--nready == 0)
					break;
			}
		}
	}
}

/**
 * @brief 创建服务器监听套接字
 *
 * @return 返回监听套接字
 */
int startup(){
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVERPORT);

	int listenfd;
	listenfd = Socket(AF_INET, SOCK_STREAM, 0);

	Bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

	Listen(listenfd, 20);

	return listenfd;
}

/**
 * @brief 读取并解析http请求行
 *
 * @param reqp http请求行结构体指针，作为回传参数。
 */
void readRequest(int fd, struct httpReq *reqp){
	int nread;
	char buff[BUFFSIZE];

	int i ,j;
	if((nread = readLine(fd, buff, sizeof(buff))) != 0){
		printf("%s", buff);

		i = j = 0;
		while(i < BUFFSIZE && buff[i] != ' '){
			reqp->method[j] = buff[i];
			j++, i++;
		}
		reqp->method[j] = '\0';

		j = 0;
		i++;
		while(i < BUFFSIZE && buff[i] != ' '){
			reqp->url[j] = buff[i];
			j++, i++;
		}
		reqp->url[j] = '\0';
	}

	fclear(fd);

}
/**
 * @brief 回应unimplementd包
 */
void unimplemented(int fd, const struct httpRes* resp){
	sendStatRow(fd, resp);
	sendResHead(fd);

	sendline(fd, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	sendline(fd, "</TITLE></HEAD>\r\n");
	sendline(fd, "<BODY><P>HTTP request method not supported.\r\n");
	sendline(fd, "</BODY></HTML>\r\n");

}
/**
 * @brief 发送notFound包
 */
void notFound(int fd, const struct httpRes* resp){
	sendStatRow(fd, resp);
	sendResHead(fd);

	sendline(fd, "<HTML><TITLE>Not Found</TITLE>\r\n");
	sendline(fd, "<BODY><P>The server could not fulfill\r\n");
	sendline(fd, "your request because the resource specified\r\n");
	sendline(fd, "is unavailable or nonexistent.\r\n");
	sendline(fd, "</BODY></HTML>\r\n");
}
/**
 * @brief 发送文件
 */
void serveFile(int fd, const struct httpRes*resp, const char* fname){
	sendStatRow(fd, resp);
	sendResHead(fd);

	FILE *resource = NULL;
	resource = fopen(fname, "r");

	char buff[BUFFSIZE];
	fgets(buff, sizeof(buff), resource);
	while(!feof(resource)){
		Writen(fd, buff, strlen(buff));
		fgets(buff, sizeof(buff), resource);
	}
	fclose(resource);
}
/**
 * @brief 发送一行字符串
 */
void sendline(int fd, const char *buff){
	Writen(fd, buff, strlen(buff));
}
/**
 * @brief 发送响应包状态行
 */
void sendStatRow(int fd, const struct httpRes *resp){
	char buff[BUFFSIZE];

	sprintf(buff, "HTTP/1.0 %s %s\r\n", resp->statCode, resp->statWord);
	Writen(fd, buff, strlen(buff));
}
/**
 * @brief 发送响应包头部
 */
void sendResHead(int fd){
	sendline(fd, SERVER_STRING);
	sendline(fd, "Content-Type:text/html\r\n");
	sendline(fd, "\r\n");
}
/**
 * @brief 判断该文件是否存在
 *
 * @return 存在返回1, 否则返回0
 */
int isExist(const char *fname){
	struct stat st;

	if(stat(fname, &st) < 0){
		return 0;
	}
	
	return 1;
}
/**
 * @brief 清空套接字缓存区
 */
void fclear(int fd){
	int flags;
	char buff[1];

	flags = fcntl(fd, F_GETFL);
	flags |= O_NONBLOCK;

	if(fcntl(fd, F_SETFL, flags) == -1){
		perr_exit("fcntl F_SETFL");
	}

	while(read(fd, buff, 1) > 0){
		printf("%s", buff);
	}
	if(errno != EAGAIN){
		perr_exit("read");
	}
	
	/*
	int numchars = 1;
	char buff[BUFFSIZE];

	buff[0] = 'A', buff[1] = '\0';
	while((numchars > 0) && strcmp("\n", buff)){
		numchars = readLine(fd, buff, sizeof(buff));
		printf("%s", buff);
	}*/
}
/**
 * @brief 读取一行数据
 */
int readLine(int sock, char *buf, int size){
	int i = 0;
	char c = '\0';
	int n;

	while((i < size - 1) && (c != '\n')){
		n = read(sock, &c, 1);
		if(n > 0){
			if(c == '\r'){
				n = recv(sock, &c, 1, MSG_PEEK);
				if((n > 0) && (c == '\n'))
					read(sock, &c, 1);
				else
					c = '\n';
			}
			buf[i] = c;
			i++;
		}
		else
			c = '\n';
	}
	buf[i] = '\0';
	
	return i;
}
