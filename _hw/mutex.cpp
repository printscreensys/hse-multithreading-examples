#include <linux/futex.h>
#include <sys/syscall.h>

#include <atomic>
#include <iostream>
#include <thread>
#include <unistd.h>

class FutexMutex {
    std::atomic<int> state{0};

    int futex(int* uaddr, int futex_op, int val) {
        return syscall(SYS_futex, uaddr, futex_op, val, nullptr, nullptr, 0);
    }

public:
    void lock() {
        int expected = 0;
        if (state.compare_exchange_strong(expected, 1, std::memory_order_acquire)) {
            return;
        }

        while (true) {
            int prev = state.exchange(2, std::memory_order_acquire);
            if (prev == 0) {
                return;
            }
            futex(reinterpret_cast<int*>(&state), FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 2);
        }
    }

    void unlock() {
        if (state.fetch_sub(1, std::memory_order_release) != 1) {
            state.store(0, std::memory_order_release);
            futex(reinterpret_cast<int*>(&state), FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1);
        }
    }
};

void foo(int64_t* n, FutexMutex* m = nullptr) {
  for (int i = 0; i < 10000000; ++i) {
    if (m != nullptr) {
      m->lock();
    }
    *n += 1;
    if (m != nullptr) {
      m->unlock();
    }
  }
}

int main(int argc, char** argv) {
  {
    int64_t wo_mutex = 0;
    std::thread t1(foo, &wo_mutex, nullptr);
    std::thread t2(foo, &wo_mutex, nullptr);
    std::thread t3(foo, &wo_mutex, nullptr);

    t1.join();
    t2.join();
    t3.join();

    std::cout << "wo_mutex  = " << wo_mutex << "\n";
  }

  {
    int64_t with_mutex = 0;
    FutexMutex m;

    std::thread t1(foo, &v, &m);
    std::thread t2(foo, &v, &m);
    std::thread t3(foo, &v, &m);

    t1.join();
    t2.join();
    t3.join();

    std::cout << "with_mutex = " << with_mutex << "\n";
  }

  return 0;
}

// wo_mutex   = 10906275
// with_mutex = 30000000