#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>

/*请求协议头
HTTP/1.1 200 OK
server:xhttpd
Date: Fri, 18 Jul 2014 14:34:26 GMT
Content-Type: text/plain; charset=iso-8859-1
Content-Length: 32
Content-Language: zh-CN
Last-Modified: Fri, 18 Jul 2014 08:36:36 GMT
Connection: close
*/

const int MAXSIZE = 4096;

// 发送错误页面
void send_error(int cfd, int status, char* title, char* text)
{
    char buf[MAXSIZE] = { 0 };
    // 发送协议头
    sprintf(buf, "%s %d %s\r\n", "HTTP/1.1", status, title);
    sprintf(buf + strlen(buf), "Content-Type:%s\r\n", "text/html");
	sprintf(buf + strlen(buf), "Content-Length:%d\r\n", -1);
	sprintf(buf + strlen(buf), "Connection: close\r\n");

    send(cfd, buf, strlen(buf), 0);
    send(cfd, "\r\n", 2, 0);

    // 发送错误页面
    memset(buf, 0, sizeof(buf));
    sprintf(buf, "<html><head><title>%d %s</title></head>\n", status, title);
	sprintf(buf + strlen(buf), "<body bgcolor = \"#cc99cc\"><h4 align=\"center\">%d %s</h4>\n", status, title);
	sprintf(buf + strlen(buf), "%s\n", text);
	sprintf(buf + strlen(buf), "<hr>\n</body>\n</html>\n");
	send(cfd, buf, strlen(buf), 0);

    return;
}

// 获取一行的输入， \r\n 结尾
int get_line(int cfd, char* buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while((i < size -1) && (c != '\n'))
    {
        n = recv(cfd, &c, 1, 0);
        if(n > 0)
        {
            // 
            if(c == '\r')
            {
                n = recv(cfd, &c, 1, MSG_PEEK);
                if((n > 0) && (c == '\n'))
                {
                    recv(cfd, &c, 1, 0);
                }
                else
                {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        }
        else
        {
            c = '\n';
        }
    }
    buf[i] = '\0';
    if(-1 == n)
    {
        i = n;
    }
    return i;
}

// 初始化套接字 socket 
int init_listen_fd(int port, int epfd)
{
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if(lfd == -1)
    {
        perror("socket error");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
	//inet_aton("172.16.0.7", &serv_addr.sin_addr);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 设置端口复用
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int ret = bind(lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if(ret == -1)
    {
        perror("bind errot");
        exit(-1);
    }

    ret = listen(lfd, 128);
    if(ret == -1)
    {
        perror("listen errot");
        exit(-1);
    }

    // 将监听描述符添加到 epoll 树上
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;

    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if(ret == -1)
    {
        perror("epoll_ctl add error");
        exit(-1);
    }    
    
    return lfd;
}

// 接受浏览器请求
void do_accept(int lfd, int epfd)
{
    struct sockaddr_in clie_addr;
    socklen_t clie_addr_len = sizeof(clie_addr);

    int cfd = accept(lfd, (struct sockaddr*)&clie_addr, &clie_addr_len);
    if(cfd == -1)
    {
        perror("accept errot");
        exit(-1);
    }

    // 打印客户端 IP + port
    char client_ip[64] = {0};
    printf("New client IP is %s, port is %d, cfd = %d\n",
            inet_ntop(AF_INET, &clie_addr.sin_addr.s_addr, client_ip, sizeof(client_ip)),
            ntohs(clie_addr.sin_port),
            cfd);

    // 设置 cfd 非阻塞
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    // 将新节点 cfd 挂到 epoll 监听树上并设置边沿模式 ET
    struct epoll_event ev;
    ev.data.fd = cfd;
    ev.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
    if(ret == -1)
    {
        perror("epoll_ctl add error");
        exit(-1);
    }
}

// 断开连接
void disconnect(int cfd, int epfd)
{
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    if(ret != 0)
    {
        perror("epoll_ctl del error");
        exit(-1);
    }
    close(cfd);
}

// 回发报文协议头
void send_respond(int cfd, int no, char* disc, const char* type, int len)
{
    char buf[1024] = {0};
    sprintf(buf, "HTTP/1.1 %d %s\r\n", no, disc);
	sprintf(buf + strlen(buf), "Content-Type: %s\r\n", type);
	sprintf(buf + strlen(buf), "Content-Length: %d\r\n", len);

    send(cfd, buf, strlen(buf), 0);
    send(cfd, "\r\n", 2, 0);    
}

// 获取文件类型
char* get_file_type(char* name)
{
    char * dot;
	char c = '.';
    dot = strrchr(name, c);

	if (dot == NULL)
	{
		return "text/plain; charset=utf-8";
	}
	else if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
	{
		return "text/html; charset=utf-8";
	}
	else if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jepg") == 0)
	{
		return "image/jpeg";
	}
	else if (strcmp(dot, ".gif") == 0)
	{
		return "image/gif";
	}
	else if (strcmp(dot, ".png") == 0)
	{
		return "image/png";
	}
	else if (strcmp(dot, ".css") == 0)
	{
		return "text/css";
	}
	else if (strcmp(dot, ".au") == 0)
	{
		return "audio/basic";
	}
	else if (strcmp(dot, ".wav") == 0)
	{
		return "audio/wav";
	}
	else if (strcmp(dot, ".avi") == 0)
	{
		return "video/x-msvideo";
	}
	else if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
	{
		return "video/quicktime";
	}
	else if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
	{
		return "video/mpeg";
	}
	else if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
	{
		return "model/vrml";
	}
	else if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
	{
		return "audio/midi";
	}
	else if (strcmp(dot, ".mp3") == 0)
	{
		return "audio/mpeg";
	}
		return "text/plain; charset=iso-8859-1";
}

// 十六进制数转换为十进制
int hexit(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

// 解码： 讲十六进制数转换为十进制
void  decode_str(char* to, char* from)
{
    for(; *from != '\0'; ++to, ++from)
    {
        if(from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            *to = hexit(from[1])*16 + hexit(from[2]);
            from += 2;
        }
        else
        {
            *to = *from;
        }
    }
    *to = '\0';
}

// 编码 ：将本地格式转换为浏览器格式
void encode_str(char* to, int to_size, const char* from)
{
    int tolen;
    for(tolen = 0; *from != '\0' && tolen + 4 < to_size; ++from)
    {
        if(isalnum(*from) || strchr("/_.-~", *from) != (char*)0)
        {
            *to = *from;
            ++to;
            ++tolen;
        }
        else
        {
            sprintf(to, "%%%02x", (int)*from & 0xff);
            to += 3;
            tolen += 3;
        }
    }

    *to = '\0';
}

void send_file(int cfd, const char* filename);
void send_dir(int cfd, const char* dirname);

// 处理http请求， 判断文件是否存在， 然后回发
void http_request(int cfd, const char* requset)
{
    // GET / HTTP/1.1, 
    char method[12], path[1024], protocol[12];
    sscanf(requset, "%[^ ] %[^ ] %[^ ]", method, path, protocol);

    printf("method = %s, path = %s, protocol = %s", method, path, protocol);
    decode_str(path, path);

    // 读取访问文件名, 为空时显示资源目录的内容
    char* file = path + 1;
    if(strcmp(path, "/") == 0)
        file = "./";

    struct stat sbuf;

    // 判断当前文件是否存在
    int ret = stat(file, &sbuf);
    if(ret == -1)
    {
        // 打开文件失败， 发送 404 页面
        send_error(cfd, 404, "NOT FOUND", "No such file or direntry");
        return;
    }

    if(S_ISDIR(sbuf.st_mode))
    {
        char* file_type;
        file_type = get_file_type(".html");
        send_respond(cfd, 200, "OK", file_type, -1);

        send_dir(cfd, file);
    }
    else if(S_ISREG(sbuf.st_mode))
    {
        char* file_type;
        file_type = get_file_type(file);
        send_respond(cfd, 200, "OK", file_type, sbuf.st_size);

        send_file(cfd, file);
    }
}

// 发送目录内容
void send_dir(int cfd, const char* dirname)
{
    int i, ret;
    char buf[4094] = {0};

	sprintf(buf, "<html><head><title>目录名：%s</title></head>", dirname);
	sprintf(buf + strlen(buf), "<body><h1>当前目录：%s</h1><table>", dirname);

    char enstr[1024] = {0};
    char path[1024] = {0};

    // 递归遍历一个目录下的子目录和文件
    struct dirent** ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort);

    for(int i = 0; i < num; ++i)
    {
        char* name = ptr[i] -> d_name;
        sprintf(path, "%s%s", dirname, name);
        printf("path = %s------------------------- \n", path);

        struct stat st;
        stat(path, &st);
        // 编码生成 %E5 %A7 等格式
        encode_str(enstr, sizeof(enstr), name);

        if(S_ISREG(st.st_mode) )
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", enstr, name, (long)st.st_size);
        else if(S_ISDIR(st.st_mode) )
            sprintf(buf + strlen(buf), "<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>", enstr, name, (long)st.st_size);

        ret = send(cfd, buf, strlen(buf), 0);
        if(ret == -1)
        {
            if(errno == EAGAIN)
            {
                perror("send error");
                continue;
            }
            else if (errno == EINTR)
			{
				perror("send error");
				continue;
			}
			else
			{
				perror("send error");
				exit(1);
			}
        }
        memset(buf, 0, sizeof(buf));
    }
    // 字符串拼接
	sprintf(buf + strlen(buf), "</table></body></html>");
	send(cfd, buf, strlen(buf), 0);
	printf("dir message send OK!!!!\n");

#if 0
    // 打开目录
    DIR *dir = opendir(dirname);
    if(dir == NULL)
    {
        perror("open dir error");
        exit(1);
    }
    // 读目录
    struct dirten* ptr = NULL;
    
    while((ptr = readaddir(dir)) != NULL)
        char* name = ptr->name;
    close(dir);
#endif
}

// 发送服务器本地文件给浏览器
void send_file(int cfd, const char* filename)
{
    // 打开文件
    int fd = open(filename, O_RDONLY);
    if(fd == -1)
    {
        send_error(cfd, 404, "Not Found", "No such file or direntry");
        exit(1);
    }

    int n = 0, ret = 0;
    char buf[4096] = { 0 };

    while((n = read(fd, buf, sizeof(buf) ) ) > 0 )
    {
        ret = send(cfd, buf, n , 0);
        if(ret == -1)
        {
            if (errno == EAGAIN)
			{
				perror("send error");
				continue;
			}
			else if (errno == EINTR)
			{
				perror("send error");
				continue;
			}
			else
			{
				perror("send error");
				exit(1);
			}
        }
    }
    if(n == -1)
    {
        perror("read file error");
        exit(1);
    }
    close(fd);
}

// 读文件操作
void do_read(int cfd, int epfd)
{
    char line[1024] = {0};
    //读 http请求协议 首行 GET/hello.c HTTP/1.1
    int len = get_line(cfd, line, sizeof(line));
    if(len == 0)
    {
        printf("client has closed!...\n");
        disconnect(cfd, epfd);
    }
    else
    {
        printf("请求行数据: %s \n", line);
        while(1)
        {
            char buf[1024] = {0};
            len = get_line(cfd, buf, sizeof(buf));
            if(buf[0] == '\n') break;
            else if(len == -1) break;
        }
    }
    if(strncasecmp("get", line, 3) == 0)
    {
        http_request(cfd, line);
        disconnect(cfd, epfd);
    }
}

// epoll 反应堆模型
void epoll_run(int port)
{
    int i = 0;
    struct epoll_event all_events[MAXSIZE];

    int epfd = epoll_create(MAXSIZE);
    if(epfd == -1)
    {
        perror("epoll_creare error");
        exit(1);
    }

    int lfd = init_listen_fd(port, epfd);

    while(1)
    {
        int ret = epoll_wait(epfd, all_events, MAXSIZE, -1);
        if(ret == -1)
        {
            perror("epoll_wait error");
            exit(1);
        }
        for(int i = 0; i < ret; i++)
        {
            struct epoll_event *pev = &all_events[i];
            if(!(pev->events & EPOLLIN ))
                continue;
            if(pev->data.fd == lfd)
                do_accept(lfd, epfd);
            else
            {
                    do_read(pev->data.fd, epfd);
            }
        }
    }

}

int main(int argc, char* argv[])
{
    if(argc < 3)
    {
        printf("./server port path\n");
    }
    int port = atoi(argv[1]);
    int ret = chdir(argv[2]);

    // 进入工作目录
    if(ret != 0)
    {
        perror("chdir error");
        exit(1);
    }

    // 设置服务器端口、启动 epoll 监听 
    epoll_run(port);

    return 0;
}        
