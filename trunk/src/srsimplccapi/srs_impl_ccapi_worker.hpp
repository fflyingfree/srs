#pragma once

#include "ccapi/srs_ccapi.hpp"

using namespace srs_ccapi;

class SrsImplCcApiWorker
{
public:
    SrsImplCcApiWorker();
    ~SrsImplCcApiWorker();

public:
    bool dostart(int evfd, int shmid);

private:
    int m_evfd;
};
