/*==============================================
 * FileName    : chat_server.c
 * Author      : liming
 * Create Time : 2019-06-17
 * description : 一个本地聊天室(利用 fifo 完成)，包含有客户端的登录，发送消息、退出。 
 ==============================================*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "chat_server.h"

// 定义一个公共 FIFO 
#define SERV_FIFO "/tmp/fifo"

// 定义数据包的大小，包含数据头int ，2个char[20] name 描述， 一个 1024 的信息段
#define BUFSIZE 1068


// 初始化，创建 公共 FIFO，非阻塞打开。设置 STDIN_FILENO 为非阻塞
void init_server(){

	umask(0);
	int ret = mkfifo(SERV_FIFO, 0777);
	if(ret < 0)
	{
		perror("create fifo:");
		exit(1);
	}

	ser_fd = open(SERV_FIFO, O_RDONLY|O_NONBLOCK);
	if(ser_fd < 0)
	{
		perror("open:");
		exit(1);
	}

	for(int i = 0; i < 100; i++)
		my_client[i].status = 0;

	fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	ser_flags = 1 - ser_flags;

	printf("server is running......\n");
}


// 接收客户端发送的数据包,
void recv_message(){
	char buf [BUFSIZE];
	MP *ms;

	int len = read(ser_fd, buf, sizeof(buf));
	if(len > 0)
	{//	 buf 是一个缓冲区，也是一个指针
		ms = (MP*)buf;
			parse_packet(ms);
	}
}


// 解析客户端的数据包，调用对应的数据
void parse_packet(MP* ms){
	switch(ms->number)
	{
		case 0:
			client_login(ms->send_name);
			break;
		case 1:
			send_message(ms);
			break;
		case 2:
			client_quit(ms->send_name);
			break;
	}
}


// 客户登录，加入到队列中，开启一个私有 FIFO
void client_login(char* login_name){
	// 找到数组中状态为 0 的位置，放入数组中。

	int index = 0;
	if(num_client < 99)
	{
		for(int i = 0; i < 100; i++ )
		{
			if(my_client[i].status == 0)
			{
				my_client[i].status = 1;
				index = i;
				break;
			}
		}

		// 记录当前已登录客户端的最大值
		if(index > max_num)
			max_num = index; 

		// 避免浅拷贝
		strcpy(my_client[index].client_name, login_name);
		char path[25] = "/tmp/";
		strcat(path, login_name);
		printf(" path is %s\n", path);

		int ret = mkfifo(path, 0777);
		if(ret < 0)
		{
			perror("create private fifo");
			exit(1);
		}
		//	加上 O_NONBLOCK 导致读取错误
		my_client[index].fifo_fd = open(path, O_WRONLY);
		if(my_client[index].fifo_fd < 0)
		{
			perror("open private fifo ");
			exit(1);
		}
		char buf[] = "Login succeful! lets begin talking!\n";
		write(my_client[index].fifo_fd, buf, sizeof(buf));

		unlink(path);
		++num_client;
	}
	// 预留最后一个位置处理客户端过多的情况
	else 
	{
		index = 99;
		strcpy(my_client[index].client_name, login_name);
		char path[25] = "/tmp/";
		strcat(path, login_name);

		mkfifo(path, 0777);
		my_client[index].fifo_fd = open(path, O_WRONLY|O_NONBLOCK);
		char buf[] = "sorry, login number overed, please try again later!\n";
		write(my_client[index].fifo_fd, buf, sizeof(buf));

		unlink(path);
		
		// 调用客户端退出函数
		client_quit(login_name);
	}

}



// 发送消息，将数据包转换成 char* 类型
void send_message(MP *ms){
	char* buf = (void*) ms;

	for(int i = 0; i <= max_num; i++)
	{
		if(strcmp(my_client[i].client_name, ms->recv_name) == 0)
		{
			write(my_client[i].fifo_fd, buf, BUFSIZE);
			break;
		}
	}

	// 群发功能
}


// 客户端退出，维护队列。登录数减 1
void client_quit(char* quit_name){
	for(int i = 0; i <= max_num; i++)
	{
		if(strcmp(my_client[i].client_name, quit_name) == 0)
		{
			close(my_client[i].fifo_fd);
			my_client[i].fifo_fd = -1;
			my_client[i].client_name[0] = '\0';
			my_client[i].status = 0;
			num_client--;
			break;
		}
	}

	printf("%s has exited\n", quit_name);
}


// 关闭服务器，关闭公共 FIFO 和其他私有 fifo 写端
void close_server(){
	char buf[] = "server will close soon...\n";
	for(int i = 0; i < max_num; i++)
	{
		if(my_client[i].status == 1)
		{
			write(my_client[i].fifo_fd, buf, sizeof(buf));
			close(my_client[i].fifo_fd);
		}
		else
			continue;
	}

	close(ser_fd);
	ser_flags = 1 - ser_flags;
	printf("server has closed!\n");
}

 
// 服务器对输入的处理
void message_handle(char* ms){
	if(strcmp(ms, "quit") == 0)
		close_server();
	else if(strcmp(ms, "number"))
		printf("current login is: %d\n", num_client);
}


int main(){
	init_server();
	char mes[10];
	
	// 服务开始执行
	while(ser_flags)
	{
		recv_message();
		if(scanf("%s", mes) != EOF)
		{
			message_handle(mes);
		}
	}

	return 0;
}
