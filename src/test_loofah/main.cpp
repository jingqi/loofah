
// see http://blog.csdn.net/luotuo44/article/details/39670221

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

#include <nut/platform/platform.h>

#if NUT_PLATFORM_OS_WINDOWS
#include <windows.h>
#endif

#include <nut/logging/logger.h>
#include <nut/threading/thread_pool.h>

#include <loofah/utils.h>

using namespace nut;

#define TAG "main"

void start_reactor_server(void*);
void start_reactor_client(void*);

static void setup_std_logger()
{
    rc_ptr<StreamLogHandler> handler = rc_new<StreamLogHandler>(std::cout);
    handler->get_filter().forbid(NULL, LL_WARN | LL_ERROR | LL_FATAL);
    handler->set_flush_mask(LL_DEBUG | LL_INFO);
    Logger::get_instance()->add_handler(handler);

    handler = rc_new<StreamLogHandler>(std::cerr);
    handler->get_filter().forbid(NULL, LL_DEBUG | LL_INFO);
    handler->set_flush_mask(LL_WARN | LL_ERROR | LL_FATAL);
    Logger::get_instance()->add_handler(handler);
}

int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    setup_std_logger();

    if (!loofah::init_network())
    {
        NUT_LOG_F(TAG, "initialize network failed");
        loofah::shutdown_network();
        return -1;
    }

    rc_ptr<ThreadPool> tp = rc_new<ThreadPool>(3);
    tp->add_task(&start_reactor_server);
    tp->add_task(&start_reactor_client);
    tp->start();
    tp->join();

    return 0;
}

NUT_LOGGING_IMPL
