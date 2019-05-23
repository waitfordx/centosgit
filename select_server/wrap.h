#ifndef WRAP_H
#define WARP_H

void perr_exit(const char *s);
int my_socket(int domain, int type, int protocol);
int my_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int my_listen(int sockfd, int backlog);
int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int my_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
void print_port_ip(struct sockaddr_in client_name, char *buf_name);
#endif
