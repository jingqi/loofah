
// see http://blog.csdn.net/luotuo44/article/details/39670221

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include <nut/logging/logger.h>
#include <nut/threading/thread_pool.h>

using namespace nut;

void start_reactor_server(void*);

int main(int argc, char **argv)
{
	rc_ptr<ThreadPool> tp = rc_new<ThreadPool>(10);
	tp->add_task(&start_reactor_server);
	tp->start();
	return 0;
}

NUT_LOGGING_IMPL
