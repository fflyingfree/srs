#include "srs_impl_ccapi_worker.hpp"

std::atomic<SrsCcApiSharedMemory*> g_srs_ccapi_shmptr(nullptr);
std::atomic<int> g_srs_ccapi_shmid(-1);

SrsImplCcApiWorker::SrsImplCcApiWorker() {
    m_evfd = -1;
}

SrsImplCcApiWorker::~SrsImplCcApiWorker() {

}

bool SrsImplCcApiWorker::dostart(int evfd, int shmid) {

}
