
#ifndef ___HEADFILE_8723819F_8E41_4D8B_AB1F_81205AC4CE38_
#define ___HEADFILE_8723819F_8E41_4D8B_AB1F_81205AC4CE38_

namespace loofah
{

// 复用监听端口，必须在 bind() 之前调用
bool make_listen_socket_reuseable(int listen_socket_fd);

// 设置非阻塞
bool make_socket_nonblocking(int socket_fd, bool nonblocking=true);

}

#endif
