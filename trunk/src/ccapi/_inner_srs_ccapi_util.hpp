#pragma once

#include <atomic>

namespace srs_ccapi
{

class srs_ccapi_SpinLocker
{
public:
    srs_ccapi_SpinLocker() : _flag(ATOMIC_FLAG_INIT) {
    }
    ~srs_ccapi_SpinLocker() {
    }

public:
    void lock() {
        while(_flag.test_and_set(std::memory_order_acquire)) {
        }
    }
    void unlock() {
        _flag.clear(std::memory_order_release);
    }

private:
    std::atomic_flag _flag;
};

};
