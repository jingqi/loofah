
// see http://blog.csdn.net/luotuo44/article/details/39670221

#include <loofah/loofah.h> // This should appear before "windows.h"
#include <nut/nut.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#   include <conio.h>
#else
#   include <unistd.h>
#endif

using namespace nut;

#define TAG "main"

void test_reactor();
void test_proactor();
void test_package_channel();

static void setup_std_logger()
{
    rc_ptr<ConsoleLogHandler> handler = rc_new<ConsoleLogHandler>();
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

    test_reactor();
    test_proactor();
    test_package_channel();

    loofah::shutdown_network();

#if NUT_PLATFORM_OS_WINDOWS
    printf("press any key to continue...");
    getch();
#endif

    return 0;
}
