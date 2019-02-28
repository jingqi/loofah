
#ifndef ___HEADFILE_702DC0E8_77C0_46E4_AFBF_99B66463825D_
#define ___HEADFILE_702DC0E8_77C0_46E4_AFBF_99B66463825D_

// NOTE This should come first
#include <loofah/loofah_config.h>

#include <string.h>

#include <nut/threading/thread_pool.h>
#include <nut/time/time_wheel.h>

#include <loofah/proactor/proactor.h>


using namespace nut;
using namespace loofah;

#define LISTEN_ADDR "localhost"
#define LISTEN_PORT 2347

struct GlobalData
{
    // params
    size_t block_size = 4096;
    int connection_num = 10;
    int seconds = 10;

    // subtotal
    size_t client_read_count = 0;
    size_t client_read_size = 0;

    size_t server_read_count = 0;
    size_t server_read_size = 0;

    // others
    TimeWheel timewheel;
    Proactor proactor;
};

extern GlobalData g_global;

void start_server();
void start_client();

#endif
