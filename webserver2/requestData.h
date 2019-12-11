// 声明和定义 请求数据结构体 和 定时器结构体

#ifndef REQUSETDATA
#define REQUSETDATA

#include <string>
#include <unordered_map>

const int STATE_PARSE_URI = 1;      // 解析地址
const int STATE_PARSE_HEADERS = 2;
const int STATE_RECV_BODY = 3;
const int STATE_ANALYSIS = 4;
const int STATE_FINISH = 5;

const int MAX_BUFF = 4096;

// 重连尝试次数
const int AGAIN_MAX_TIMES = 200;

// 解析地址
const int PARSE_URI_AGAIN = -1;
const int PARSE_URI_ERROR = -2;
const int PARSE_URI_SUCCESS = 0;

// 解析头
const int PARSE_HEADER_AGAIN = -1;
const int PARSE_HEADER_ERROR = -2;
const int PARSE_HEADER_SUCCESS = 0; 

// 分析数据
const int ANALYSIS_ERROR = -2;
const int ANALYSIS_SUCCESS = 0;

// HTTP 请求类型  POST 方法还是 GET 方法
const int METHOD_POST = 1;
const int METHOD_GET = 2;
const int HTTP_10 = 1;
const int HTTP_11 = 2;

const int EPOLL_WAIT_TIME = 500;

// 用来判断请求文件名后缀
class MimeType
{
private:
    static pthread_mutex_t lock;
    static std::unordered_map<std::string, std::string> mime;
    MimeType();
    MimeType(const MimeType &m);
public:
    static std::string getMime(const std::string &suffinx);
};

// 请求头状态
enum HeaderState
{
    h_start = 0,
    h_key,
    h_colon,
    h_spaces_after_colon,
    h_value,
    h_CR,
    h_LF,
    h_end_CR,
    h_end_LF
};

struct mytimer;
struct requestData;

// 请求数据
struct requestData
{
private:
    int againTimes;  // 观察请求次数
    std::string path;
    int fd;
    int epollfd;
    std::string content;
    int method;
    int HTTPversion;
    std::string file_name;
    int now_read_pos;
    int state;
    int h_state;
    bool isfinish;
    bool keep_alive;
    std::unordered_map<std::string, std::string> headers;

    mytimer *timer;

private:
int parse_URL();

int parse_Headers();

int analysisRequest();

public:
    requestData();
    requestData(int _epollfd, int _fd, std::string _path);
    ~requestData();
    void addTimer(mytimer* mtimer);
    void reset();
    void seperateTimer();
    int getFd();
    void setFd(int _fd);
    void handleRequset();
    void handleError(int fd, int err_num, std::string short_msg);
};

// 定时器结构体
struct mytimer
{
    bool deleted;
    size_t expired_time;
    requestData *requset_data;

    mytimer(requestData* _request_data, int timeout);
    ~mytimer();
    void update(int timeout);
    bool isvalid();
    void clearReq();
    bool isDeleted() const;
    size_t getExpTime() const;
};

// 设置定时器比较类，重载括号操作符 
struct timerCmp
{
    // a 的定时器存活时间大于 b ,返回 true
    bool operator()(const mytimer* a, const mytimer* b) const;
};
#endif