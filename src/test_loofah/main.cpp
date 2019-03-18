
// see http://blog.csdn.net/luotuo44/article/details/39670221

#include <loofah/loofah.h> // This should appear before "windows.h"
#include <nut/nut.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iostream>

#if NUT_PLATFORM_OS_WINDOWS
#   include <windows.h>
#   include <conio.h>
#else
#   include <unistd.h>
#endif

using namespace nut;
using namespace std;

#define TAG "main"

void test_reactor_manually();
void test_react_package_manually();
void test_proact_package_manually();

static void setup_std_logger()
{
    rc_ptr<ConsoleLogHandler> handler = rc_new<ConsoleLogHandler>(false);
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

    ConsoleTestLogger l;
    TestRunner runner(&l);
    runner.run_group("all");
    // runner.run_fixture("TestProactPackageRST");
    // runner.run_case("TestConcurrentHashMap", "test_multi_thread");

    // 手工测试
    // test_reactor_manually();
    // test_react_package_manually();
    // test_proact_package_manually();

    loofah::shutdown_network();

#if NUT_PLATFORM_OS_WINDOWS
    printf("press any key to continue...");
    getch();
#endif

    return 0;
}
