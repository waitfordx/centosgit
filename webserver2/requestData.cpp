#include "requestData.h"
#include "util.h"
#include "epoll.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <queue>
#include <sys/types.h>
#include <sys/socket.h>

// 打开显示文件内容
#include <dirent.h>
#include <opencv/cv.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>

using namespace cv;

#include <iostream>
using namespace std;
// 初始化锁 PTHREAD_MUTEX_INITIALZER 则是一个结构常亮
pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
// 全局锁可以直接利用宏定义初始化，而动态锁需要利用锁初始化函数进行初始化
pthread_mutex_t MimeType::lock = PTHREAD_MUTEX_INITIALIZER;
// 设定全局变量
std::unordered_map<std::string, std::string> MimeType::mime;

// mime类的实现、 MimeType 即客户端请求文件类型
std::string MimeType::getMime(const std:: string &suffix)
{
    if(mime.size() == 0)
    {
        pthread_mutex_lock(&lock);
        if(mime.size() == 0)
        {
            // 向 hashmap中加入
            mime[".html"] = "text/html; charset=utf-8";
            mime[".htm"] = "text/html; charset=utf-8";
            mime[".css"] = "text/css";
            mime[".avi"] = "video/x-msvideo";
            mime[".au"] = "audio/basic";
            mime[".bmp"] = "image/bmp";
            mime[".c"] = "text/plain";
            mime[".doc"] = "application/msword";
            mime[".gif"] = "image/gif";
            mime[".gz"] = "application/x-gzip";
            mime[".ico"] = "application/x-ico";
            mime[".jpg"] = "image/jpeg";
            mime[".png"] = "image/png";
            mime[".txt"] = "text/plain";
            mime[".mp3"] = "audio/mp3";
            mime[".mpeg"] = "video/mpeg";
            mime[".mpe"] = "video/mpeg";
            mime[".vrml"] = "model/vrml";
            mime[".wrl"] = "model/vrml";
            mime[".midi"] = "audio/midi";
            mime[".mid"] = "audio/midi";
            mime[".mov"] = "video/quicktime";
            mime[".wav"] = "audio/wav";
            mime[".qt"] = "video/quicktime";
            mime[".wav"] = "audio/wav";
            mime[".avi"] = "video/x-msvideo";
            mime["default"] = "text/html; charset=utf-8";
        }
        pthread_mutex_unlock(&lock);
    }
    if(mime.find(suffix) == mime.end())
        return mime["defalut"];
    else
        return mime[suffix];
}

// 定时器优先级队列
priority_queue<mytimer*, deque<mytimer*>, timerCmp> myTimerQueue;

// 初始化请求数据
requestData::requestData(): 
now_read_pos(0), state(STATE_PARSE_URI), h_state(h_start), keep_alive(false), againTimes(0), timer(NULL)
{
    cout<<"requestData constructed"<<endl;
}

requestData::requestData(int _epollfd, int _fd, std::string _path):
	now_read_pos(0), state(STATE_PARSE_URI), h_state(h_start), keep_alive(false), againTimes(0), timer(NULL),
	path(_path), fd(_fd), epollfd(_epollfd)
{

}

requestData::~requestData()
{
	cout << "~requestData()" << endl;
	struct epoll_event ev;
	//超时请求一定是读操作 没有被动写
	ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	ev.data.ptr = (void*)this;
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
	if (timer != NULL)
	{
		timer->clearReq();
		timer = NULL;
	}
	close(fd);
}

// 添加定时器
void requestData::addtimer(mytimer* mtimer)
{
    if(timer == NULL)
        timer = mtimer;
}

// 得到文件描述符
int requestData::getFd()
{
    return fd;
}

// 重置请求内容
void requestData::reset()
{
    againTimes = 0;
    content.clear();
    file_name.clear();
    path.clear();
    now_read_pos = 0;
    state = STATE_PARSE_URI;
    h_state = h_start;
    headers.clear();
    keep_alive = false;
}

// 分离定时器 -- 如果定时器不为空 对定时器进行清空操作
void requestData::seperateTimer()
{
    if(timer)
    {
        timer->clear();
        timer = NULL;
    }
}

// 处理请求
void requestData::handleRequset()
{
    char buff[MAX_BUFF];
    bool isError = false;
    while(true)
    {
        int read_num = readn(fd, buf, MAX_BUFF);
        if(read_num < 0)
        {
            perror("1 readn failed");
            isError = true;
            break;
        }
        // 有请求出现但是读不到数据
        else if(read_num == 0)
        {
            perror("read_num == 0");
            if(errno == EAGAIN)
            {
                if(againTimes > AGAIN_MAX_TIMES)
                    isError = true;
                else
                {
                    ++ againTimes;
                }
            }
            else if(errno != 0)
                isError = true;

            break;
        }
        // 当前已经读到的内容
        string now_read(buff, buff + read_num);
        content += now_read;
        
        if(state == STATE_PARSE_URI)
        {
            int flag = this->parse_URL();
            if(flag == PARSE_URI_AGAIN)
                break;
            else if(flag == PARSE_URI_ERROR)
            {
                perror("2 parse uri error");
                isError = true;
                break;
            }
        }

        if(state == STATE_PARSE_HEADERS)
        {
            int flag = this->parse_Headers();
            if(flag == PARSE_URI_AGAIN)
                break;
            else if(flag == PARSE_HEADER_ERROR)
            {
                perror("3 parse header failed");
                isError = true;
                break;
            }
            if(method == METHOD_POST)
            {
                state = STATE_RECV_BODY;
            }
            else
            {
                state = STATE_ANALYSIS;
            }
        }
        if(state == STATE_RECV_BODY)
        {
            int content_length = -1;
            if(headers.find("Content-length") != headers.end())
            {
                content_length = stoi(headers["Content-length"]);
            }
            else
            {
                isError = true;
                break;
            }
            if(content.size() < content_length)
            {
                continue;
            }
            state = STATE_ANALYSIS;
        }

        if(state == STATE_ANALYSIS)
        {
            int flag = this->analysisRequest();
            if(flag < 0)
            {
                isError = true;
                break;
            }
            else if(flag == ANALYSIS_SUCCESS)
            {
                state = STATE_FINISH;
                break;
            }
            else
            {
                isError = true;
                break;
            }
        }
    }
    if(isError)
    {
        delete this;
        return;
    }
    // 加入 epoll 继续
    if(state == STATE_FINISH)
    {
        if(keep_alive)
        {
            printf("ok\n");
            this->reset(); 
        }
        else
        {
            delete this;
            return;
        }
    }

    // 先加入时间信息，
    pthread_mutex_lock(&qlock);
    mytimer *mtimer = new mytimer(this, 500);
    timer = mtimer;
    myTimerQueue.push(mtimer);
    pthread_mutex_unlock(&qlock);

    __uint32_t _epo_event = EPOLLIN | EPOLLET | EPOLLONESHOT;
    int ret = epoll_mod(epollfd, fd, static_cast<void*>(this), _epo_event);
    if(ret < 0)
    {
        delete this;
        return;
    }
}

// 16 进制数的转化
int hexit(char c)
{
	if (c >= '0' && c <= '9')
	{
		return c - '0';
	}
	if (c >= 'a' && c <= 'f')
	{
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F')
	{
		return c - 'A' + 10;
	}
	return 0;
}

//解码
void decode_str(char *to, char *from)
{
	for (; *from != '\0'; ++to, ++from)
	{
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
		{
			*to = hexit(from[1]) * 16 + hexit(from[2]);
			from += 2;
		}
		else
		{
			*to = *from;
		}
	}
	*to = '\0';
}

//编码
void encode_str(char *to, int tosize, const char* from)
{
	int tolen;
	for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from)
	{
		if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0)
		{
			*to = *from;
			++to;
			++tolen;
		}
		else
		{
			//%%表示转义的意思
			sprintf(to, "%%%02x", (int)*from & 0xff);
			to += 3;
			tolen += 3;
		}
	}
	*to = '\0';
}

// 解析地址
int requestData::parse_URL()
{
    string &str = content;

    int pos = str.find('\r', now_read_pos);
    if(pos < 0)
    {
        return PARSE_URI_AGAIN;
    }
    // 去掉请求行所占的空间。
    string request_line = str.substr(0, pos);
    if(str.size() > pos +1);
    {
        str = str.substr(pos + 1);
    }
    else
    {
        str.clear();
    }
    // 拆分 http 请求行
    //char method1[12], path1[1024], protocol[12];
	//sscanf(request_line.c_str(), "%[^ ] %[^ ] %[^ ]", method1, path1, protocol);
	//printf("method = %s, path = %s, protocol = %s", method1, path1, protocol);
	//method = GET, path = /mm.jpg, protocol = HTTP/1.1ok

    // GET 请求方法 ：Method GET http://www.facebook.com/ HTTP/1.1
    pos = request_line.find("GET");
    if(pos < 0)
    {
        pos = request_line.find("POST");
        if(pos < 0)
        {
            return PARSE_URI_ERROR;
        }
        else
        {
            method = METHOD_POST
        }
    }
    else
    {
        method = METHOD_GET;
    }
    
    //filename GET http://www.facebook.com/ HTTP/1.1
    pos = request_line.find("/", pos);
    if(pos < 0)
    {
        return PARSE_URI_ERROR;
    }
    else
    {
        int _pos = request_line.find(' ', pos);
        if(_pos < 0)
        {
            return PARSE_URI_ERROR;
        }
        else
        {
            if(_pos - pos > 1)
            {
                file_name = requset_line.substr(pos + 1, _pos - pos - 1);
                //输出string字符串的正确方式 printf("file_name = %s", file_name.c_str());

                // 解码： string -》 const char* -> char* -> string
                char* tmp = (char*)file_name.c_str();
                decode_str(tmp, tmp);
                file_name = (string)tmp;

                int __pos = file_name.find('?');
                if(__pos >= 0)
                {
                    file_name = file_name.substr(0, __pos);
                }
            }
            else
            {
                file_name = "./";
            }    
        }
        pos = _pos;
    }
    // HTTP 版本号
    pos = request_line.find("/", pos);
    if(pos < 0)
    {
        return PARSE_URI_ERROR;
    }
    else
    {
        if(request_line.size() - pos <= 3)
        {
            return PARSE_URI_ERROR;
        }
        else
        {
			string ver = request_line.substr(pos + 1, 3);
			if (ver == "1.0")
			{
				HTTPversion = HTTP_10;
			}
			else if (ver == "1.1")
			{
				HTTPversion = HTTP_11;
			}
			else
			{
				return PARSE_URI_ERROR;
			}
        }
    }
    state = STATE_PARSE_HEADERS;
	return PARSE_URI_SUCCESS;
}

// 解析请求头
/*
Accept: application/x-ms-application, image/jpeg, application/xaml+xml, [...]
Accept-Language: en-US
User-Agent: Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64; [...]
Accept-Encoding: gzip, deflate
Connection: Keep-Alive
Cookie: lsd=XW[...]; c_user=21[...]; x-referer=[...]
*/
int requestData::parse_Headers()
{
    string &str = content;
    int key_start = -1, key_end = -1; value_start = -1, value_end = -1;
    int now_read_line_begin = 0;
    bool notFinish = true;
    for(int i = 0; i < str.size() && notFinish; ++i)
    {
        switch(h_state)
        {
            case h_start:
            {
                if(str[i] == '\n' || str[i] == '\r')
                {
                    break;
                }
                h_state = h_key;
                key_start = i;
                now_read_line_begin = i;
                break;
            }
            case h_key:
			{
				if (str[i] == ':')
				{
					key_end = i;
					if (key_end - key_start <= 0)
					{
						return PARSE_HEADER_ERROR;
					}
					h_state = h_colon;
				}
				else if (str[i] == '\n' || str[i] == '\r')
				{
					return PARSE_HEADER_ERROR;
				}
				break;
			}
			case h_colon:
			{
				if (str[i] == ' ')
				{
					h_state = h_spaces_after_colon;
				}
				else
				{
					return PARSE_HEADER_ERROR;
				}
				break;
			}
			case h_spaces_after_colon:
			{
				h_state = h_value;
				value_start = i;
				break;
			}
			case h_value:
			{
				if (str[i] == '\r')
				{
					h_state = h_CR;
					value_end = i;
					if (value_end - value_start <= 0)
					{
						return PARSE_HEADER_ERROR;
					}
				}
				else if (i - value_start > 255)
				{
					return PARSE_HEADER_ERROR;
				}
				break;
			}
			case h_CR:
			{
				if (str[i] == '\n')
				{
					h_state = h_LF;
					//将str中的key_start位置到key_end之间进行截取放到key中
					string key(str.begin() + key_start, str.begin() + key_end);
					string value(str.begin() + value_start, str.begin() + value_end);
					headers[key] = value;//用hash表headers来存储
					now_read_line_begin = i;
				}
				else
				{
					return PARSE_HEADER_ERROR;
				}
				break;
			}
			case h_LF:
			{
				if (str[i] == '\r')
				{
					h_state = h_end_CR;
				}
				else
				{
					key_start = i;
					h_state = h_key;
				}
				break;
			}
			case h_end_CR:
			{
				if (str[i] == '\n')
				{
					h_state = h_end_LF;
				}
				else
				{
					return PARSE_HEADER_ERROR;
				}
				break;
			}
			case h_end_LF:
			{
				notFinish = false;
				key_start = i;
				now_read_line_begin = i;
				break;
			}
        }
    }
    // 请求头已经全部正常读完
    if(h_state == h_end_LF)
    {
        str = str.substr(now_read_line_begin);
        return PARSE_URI_SUCCESS;
    }
    str = str.substr(now_read_line_begin);
    return PARSE_HEADER_AGAIN;
}

// 分析请求数据 
int requestData::analysisRequest()
{
    if(method == METHOD_POST)
    {
        char header[MAX_BUFF];

        sprintf(header, "HTTP/1.1 %d %s\r\n", 200, "OK");
        /*
        一般情况下使用的将都是长连接方式 http1.1开始就是默认长连接方式
        如果请求头为长连接方式时，那么我们将设置返回的请求头为长连接方式，否则将会使用短连接的方式
        */
        if(headers.find("Connection") != headers.end() && headers["Connection"] == "Keep-alive")
        {
            keep_alive = true;
            sprintf(header, "%sConnection: keep-alive\r\n", header);
            sprintf(header, "%sKeep-Alive: timeout=%d\r\n", header, EPOLL_WAIT_TIME);
        }
        char *send_content = "I have receiced this.";
        sprintf(header, "%sContent-length: %zu\r\n", header, strlen(send_content));
        sprintf(header, "%s\r\n", header);
        size_t send_len = (size_t)writen(fd, header, strlen(header));

        // 发送失败
        if (send_len != strlen(header))
        {
            perror("Send header failed");
            return ANALYSIS_ERROR;
        }
        send_len = (size_t)writen(fd, send_content, strlen(send_content));
        if (send_len != strlen(send_content))
        {
            perror("Send content failed");
            return ANALYSIS_ERROR;
        }
        // ?
        cout << "content size ==" << content.size() << endl;
        vector<char> data(content.begin(), content.end());
        Mat test = imdecode(data, CV_LOAD_IMAGE_ANYDEPTH | CV_LOAD_IMAGE_ANYCOLOR);
        imwrite("receive.bmp", test);
        return ANALYSIS_SUCCESS;
    }
    else if(method == METHOD_GET)
    {
        char header[MAX_BUFF];
		sprintf(header, "HTTP/1.1 %d %s\r\n", 200, "OK");
		/*
		一般情况下使用的将都是长连接方式 http1.1开始就是默认长连接方式
		如果请求头为长连接方式时，那么我们将设置返回的请求头为长连接方式，否则将会使用短连接的方式
		*/
		if (headers.find("Connection") != headers.end() && headers["Connection"] == "keep-alive")
		{
			keep_alive = true;
			sprintf(header, "%sConnection: keep-alive\r\n", header);
			//在HTTP 1.1版本后，默认都开启Keep-Alive模式，只有加入加入 Connection: close才关闭连接，当然也可以设置Keep-Alive模式的属性，
			//例如 Keep-Alive: timeout=5, max=100，表示这个TCP通道可以保持5秒，max=100，表示这个长连接最多接收100次请求就断开。
			sprintf(header, "%sKeep-Alive: timeout=%d\r\n", header, EPOLL_WAIT_TIME);
		}
        // 截取文件名
        int dot_pos = file_name.find('.');
        const char* filetype;

        if(dot_pos < 0)
            filetype = MimeType::getMime("default").c_str();
        else
            filetype = MimeType::getMime(file_name.substr(dot_pos)).c_str();

        struct stat sbuf;

        // 判断请求内容是否为目录
        if(stat(file_name.c_str(), &sbuf) < 0)
        {
            handleError(fd, 404, "NOT FOUND");
            return ANALYSIS_ERROR;
        }

        if(S_ISDIR(sbuf, st_mode))
        {
            sprintf(header, "%sContent-type: %s\r\n", header, filetype);
            sprintf(header, "%sContent-length: %ld\r\n", header, sbuf.st_size);
			//sprintf(header, "%sContent-length: %ld\r\n", header, -1);
			/*可以采用传-1的方式对Content-length的值进行设置*/
			sprintf(header, "%s\r\n", header);
            size_t send_len = (size_t)write(fd, header, strlen(header));
            if(send_len != strlen(header))
            {
                perror("Send header failed");
                return ANALYSIS_ERROR;
            }
            
            // 头已经发送， 拼接 html 页面发送给客户端
            char buf[4094] = {0};
            int i, ret;
            sprintf(buf, "<html><head><title>目录名：%s</title></head>", file_name.c_str());
			sprintf(buf + strlen(buf), "<body><h1>当前目录：%s</h1><table>", file_name.c_str());
            char enstr[1024] = { 0 };
            char path[1024] = { 0 };
            
            // 对目录进行遍历， 需要一个目录项二级指针
            struct dirent** ptr;
            int num = scandir(file_name.c_str(), &ptr, NULL, alphasort);
            for(i = 0; i < num; ++i)
            {
                char* name = ptr[i]->d_name;
                sprintf(path, "%s%s", file_name.c_str(), name);
                printf("path = %s =====================\n", path);
                struct stat st;
                stat(path, &st);

                encode_str(enstr, sizeof(enstr), name);

                if(S_ISREG(st.st_mode))
                    sprintf(buf + strlen(buf), "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>", enstr, name, (long)st.st_size);
                
                else if(S_ISDIR(st.st_mode))
                    sprintf(buf+strlen(buf), "<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>", enstr, name, (long)st.st_size);
                
                ret = send(fd, buf, strlen(buf), 0);
                if(ret == -1)
                {
                    if(errno == EAGAIN)
                    {
                        perror("send error");
                        continue;
                    }
                    else if(errno = EINTR)
                    {
                        perror("send error");
                        continue;
                    }
                    else
                    {
                        perror("send error");
                        exit(1);
                    }
                }// if(ret == -1)

                memset(buf, 0, sizeof(buf));
            }// for 遍历目录
            sprintf(buf + strlen(buf), "</table></body></html>");
			send(fd, buf, strlen(buf), 0);
			printf("dir message send OK!!!!\n");
			return ANALYSIS_SUCCESS;
        }// if(S_ISDIR()) 

        else
        {
            sprintf(header, "%sContent-type: %s\r\n", header, filetype);
			// 通过Content-length返回文件大小
			sprintf(header, "%sContent-length: %ld\r\n", header, sbuf.st_size);
			//sprintf(header, "%sContent-length: %ld\r\n", header, -1);

			sprintf(header, "%s\r\n", header);
			size_t send_len = (size_t)writen(fd, header, strlen(header));
			if (send_len != strlen(header))
			{
				perror("Send header failed");
				return ANALYSIS_ERROR;
			}
			int src_fd = open(file_name.c_str(), O_RDONLY, 0);
			char *src_addr = static_cast<char*>(mmap(NULL, sbuf.st_size, PROT_READ, MAP_PRIVATE, src_fd, 0));
			close(src_fd);
			// 发送文件并校验完整性
			send_len = writen(fd, src_addr, sbuf.st_size);
			if (send_len != sbuf.st_size)
			{
				perror("Send file failed");
				return ANALYSIS_ERROR;
			}
			/*函数说明 munmap()用来取消参数start所指的映射内存起始地址，参数length则是欲取消的内存大小。
			当进程结束或利用exec相关函数来执行其他程序时，映射内存会自动解除，但关闭对应的文件描述符时不会解除映射。*/
			munmap(src_addr, sbuf.st_size);
			return ANALYSIS_SUCCESS;
        } // else 遍历到文件
        
    }//  else if(method == METHOD_GET)

    else
    {
        return ANALYSIS_ERROR;
    }
}// int requestData::analysisRequest() 分析请求数据

// 错误情况处理
void requestData::handleError(int fd, int err_num, str::string short_msg)
{
    short_msg = " " + short_msg;
    char send_buff[MAX_BUFF];
    string body_buf, header_buf;

    body_buff += "<html><title>TKeed Error</title>";
	body_buff += "<body bgcolor=\"ffffff\">";
	body_buff += "<h4 align=\"center\"><font color=\"#FF0000\">" + to_string(err_num) + short_msg + "</font></h4>";
	body_buff += "<hr><h4 align=\"center\"><em><font color=\"#0000FF\">jacob's Web Server</font></em></h4>\n</body></html>";

    header_buff += "HTTP/1.1 " + to_string(err_num) + short_msg + "\r\n";
	header_buff += "Content-type: text/html\r\n";
	header_buff += "Connection: close\r\n";
	header_buff += "Content-length: " + to_string(body_buff.size()) + "\r\n";
	//header_buff += "Content-length: " + to_string(-1) + "\r\n";
	header_buff += "\r\n";

    sprintf(send_buff, "%s", header_buff.c_str());
	writen(fd, send_buff, strlen(send_buff));//先发请求头
	sprintf(send_buff, "%s", body_buff.c_str());
	writen(fd, send_buff, strlen(send_buff));//再发出body信息数据
}// handleError(int fd, int err_num, str::string short_msg)

// mytimer 构造函数
mytimer::mytimer(requestData* _request_data, int timeout) : deleted(false), request_data(_request_data)
{
    /*struct timeval
	{
		__time_t tv_sec;        //Seconds. 秒
		__suseconds_t tv_usec;  // Microseconds. 微秒 
	};*/
    struct timeval now;
    // int gettimeofday(struct timeval *tv, struct timezone *tz);成功返回 0  失败返回 -1
	gettimeofday(&now, NULL);
    // 以毫秒计算
    expired_time = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) + timeout;
}// mytimer 构造函数

// mytimer 析构函数
void mytimer::~mytimer()
{
    cout << "~mytimer()"<<endl;
    if(request_data != NULL)
    {
        cout<<"request_data = " << request_data << endl;
        delete request_data;
        request_data = NULL;    // 避免野指针 
    }
}

// 更新定时器的过期时间
void mytimer::update(int timeout)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    expired_time = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) + timeout;
}

bool mytimer::isvalid()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    size_t temp = ((now.tv_sec * 1000) + (now.tv_usec));
    if(temp < expired_time) return true;
    else
    {
        this->setDeleted();
        return false;
    }
}

void mytimer::clearReq()
{
    request_data = NULL;
	this->setDeleted();
}

void mytimer::setDeleted()
{
    deleted = true;
} 

bool mytimer::isDeleted() const
{
    return deleted;
}

size_t mytimer::getExpTime() const
{
    return expired_time;
}

bool timerCmp::operator()(const mytimer *a, const mytimer *b) const
{
    return a->getExpTime() > b->getExpTime();
}

