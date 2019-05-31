
#include <loofah/loofah.h>
#include <nut/nut.h>

#include <assert.h>
#include <string.h>
#include <iostream>

#if NUT_PLATFORM_OS_WINDOWS
#   include <conio.h>
#endif

#include "declares.h"


#define TAG "pingpong"

using namespace std;

GlobalData g_global;

static void setup_std_logger()
{
    rc_ptr<ConsoleLogHandler> handler = rc_new<ConsoleLogHandler>();
    Logger::get_instance()->add_handler(handler);
}

static void print_help()
{
    cout << "test_pingpong" << endl <<
        " -h" << endl <<
        " -b BLOCK_SIZE" << endl <<
        " -c CONNECTION_NUM" << endl <<
        " -s WAIT_SECONDS" << endl;
}

static int parse_params(int argc, char *argv[])
{
    assert(nullptr != argv);
    if (argc <= 1)
        return 0;

    for (int i = 1; i < argc; ++i)
    {
        if (0 == ::strcmp(argv[i], "-h"))
        {
            print_help();
        }
        else if (0 == ::strcmp(argv[i], "-b"))
        {
            if (i + 1 < argc)
            {
                g_global.block_size = (int) str_to_long(argv[i + 1]);
            }
            else
            {
                cerr << "expect block size number!" << endl;
                return -1;
            }
            ++i;
        }
        else if (0 == ::strcmp(argv[i], "-c"))
        {
            if (i + 1 < argc)
            {
                g_global.connection_num = (int) str_to_long(argv[i + 1]);
            }
            else
            {
                cerr << "expect connection number!" << endl;
                return -1;
            }
            ++i;
        }
        else if (0 == ::strcmp(argv[i], "-s"))
        {
            if (i + 1 < argc)
            {
                g_global.seconds = (int) str_to_long(argv[i + 1]);
            }
            else
            {
                cerr << "expect seconds value!" << endl;
                return -1;
            }
            ++i;
        }
        else
        {
            cerr << "unknonw option: " << argv[i] << endl;
            return -1;
        }
    }
    return 0;
}

std::string size_to_str(size_t size)
{
    if (size < 1024)
        return llong_to_str(size);

    double value = size;
    const char *unit = nullptr;

    value /= 1024;
    unit = "KB";

    if (value > 1024)
    {
        value /= 1024;
        unit = "MB";
    }

    if (value > 1024)
    {
        value /= 1024;
        unit = "GB";
    }

    char buf[256];
    ::sprintf(buf, "%.2f%s", value, unit);
    return buf;
}

static void report()
{
    cout << "-------------------------" << endl <<
        "thread: " << 1 << endl <<
        "connection: " << g_global.connection_num << endl <<
        "block size: " << g_global.block_size << endl <<
        "time: " << g_global.seconds << "s" << endl <<
        "server received count: " << g_global.server_read_count << endl <<
        "server received size: " << size_to_str(g_global.server_read_size) << endl <<
        "client received count: " << g_global.client_read_count << endl <<
        "client received size: " << size_to_str(g_global.client_read_size) << endl <<
        "operation per second: " << g_global.server_read_count / g_global.seconds << endl <<
        "bytes per second: " << size_to_str(g_global.server_read_size / g_global.seconds) << endl;
}

int main(int argc, char *argv[])
{
    const int rs = parse_params(argc, argv);
    if (rs < 0)
        return rs;

    setup_std_logger();

    if (!init_network())
    {
        NUT_LOG_F(TAG, "initialize network failed");
        shutdown_network();
        return -1;
    }

    start_server();
    start_client();
    g_global.timewheel.add_timer(
        g_global.seconds * 1000, 0,
        [=](nut::TimeWheel::timer_id_type id, uint64_t expires)
        {
            g_global.proactor.shutdown_later();
        });

    while (true)
    {
        const uint64_t idle_ms = std::min<uint64_t>(
            60 * 1000, std::max<uint64_t>(uint64_t(TimeWheel::RESOLUTION_MS), g_global.timewheel.get_idle()));
        if (g_global.proactor.poll(idle_ms) < 0)
            break;
        g_global.timewheel.tick();
    }
    g_global.proactor.shutdown_later();

    shutdown_network();
    report();

#if NUT_PLATFORM_OS_WINDOWS
    printf("press any key to continue...");
    getch();
#endif

    return 0;
}
