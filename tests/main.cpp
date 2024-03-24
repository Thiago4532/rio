#include <cstring>
#include <iostream>
#include "rio/event_loop.hpp"
#include "rio/task.hpp"
#include <poll.h>

using namespace std;
using namespace rio;

int non_blocking_read(int fd, void* buf, size_t size) {
    pollfd pfd = {fd, POLLIN | POLLPRI, 0};
    int n = poll(&pfd, 1, 0);
    if (n == -1) {
        perror("poll failed\n");
        terminate();
    }

    if (n == 0) {
        errno = EAGAIN;
        return -1;
    }

    return read(fd, buf, size);
}

task<int> re(void* buf, size_t size) {
    for(;;) {
        int n = non_blocking_read(0, buf, size);
        if (n >= 0)
            co_return n;

        if (errno == EAGAIN) {
            co_await get_event_loop().await_read(0);   
        } else {
            co_return n;
        }
    }
}

task<void> f(const char* name, int contador) {
    while (contador --> 0) {
        cout << name << ": " << contador << "\n";
        co_await sleep_for(1s);
    }
}

task<> funcao() {
    get_event_loop().add_fd(0, file_ops::readable);

    char buf[1024];
    for(;;) {
        int n = co_await re(buf, sizeof(buf) - 1);
        if (n == -1) {
            perror("read failed\n");
            break;
        }
        if (n == 0)
            break;

        buf[n] = '\0';
        if (strcmp(buf, "exit\n") == 0) {
            cout << "Exiting\n";
            break;
        }
        cout << "Read: " << buf;
        cout.flush();
    }

    get_event_loop().del_fd(0);
    co_return;
}

task<> funcao2() {
    for(int i = 0; i < 3; i++) {
        cout << "Hello\n";
        co_await sleep_for(1s);
    }
}

int main() {
    event_loop_t loop;
    loop.schedule(funcao);
    loop.schedule(funcao2);

    loop.run();
    return 0;
}
