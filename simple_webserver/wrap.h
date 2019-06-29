/*==============================================
* FileName    : wrap.h
* Author      : liming
* Create Time : 2019-06-25
* description : 声明相关函数和出错处理 
==============================================*/
#ifndef WRAP_H
#define WRAP_H

// 发送 HTTP 协议头,写到标准输出当中。让 xinetd 能够读取
void send_header(char* type);


// 发送错误显示页面
void send_error(char* err_num, char* err_discrible, char* err_content);


#endif
