#include "srs_app_zz_ccapi_bypasser_mgr.hpp"
#include "srs_app_zz_ccapi_impl.hpp"
#include "srs_kernel_codec.hpp"
#include "srs_protocol_format.hpp"

class SrsCcApiInnerStreamSourceInfo
{
public:
    friend class SrsCcApiByPasserMgr;
    SrsCcApiInnerStreamSourceInfo() {

    }
    virtual ~SrsCcApiInnerStreamSourceInfo() {

    }

private:
    SrsRtmpFormat _rtmp_format;
};

SrsCcApiByPasserMgr gSrsCcApiByPasserMgr;

SrsCcApiByPasserMgr::SrsCcApiByPasserMgr() {

}

SrsCcApiByPasserMgr::~SrsCcApiByPasserMgr() {

}

void SrsCcApiByPasserMgr::toPassRtmpVideoFrame(const std::string& streamId, SrsSharedPtrMessage* frameMsg) {
    srs_trace("xxxxxxxxxxxxx toPassRtmpVideoFrame 001 %s", streamId.c_str());
    if(!gSrsCcApiImplWorker.ison() || streamId == "" || frameMsg == nullptr) {
        return;
    }
    srs_trace("xxxxxxxxxxxxx toPassRtmpVideoFrame 002 %s", streamId.c_str());
    std::shared_ptr<SrsCcApiInnerStreamSourceInfo> srcInfoPtr = touchStreamSourceInfo(streamId, true);
    if(!srcInfoPtr) {
        return;
    }
    srs_trace("xxxxxxxxxxxxx toPassRtmpVideoFrame 003 %s", streamId.c_str());
    bool is_sequence_header = SrsFlvVideo::sh(frameMsg->payload, frameMsg->size);
    if(srcInfoPtr->_rtmp_format->on_video(frameMsg) != srs_success) {
        return;
    }
    srs_trace("xxxxxxxxxxxxx toPassRtmpVideoFrame 004 %s", streamId.c_str());
    if(!srcInfoPtr->_rtmp_format->vcodec || !srcInfoPtr->_rtmp_format->video) {
        return;
    }
    srs_trace("xxxxxxxxxxxxx toPassRtmpVideoFrame 005 %s", streamId.c_str());
    long msgId = gSrsCcApiImplWorker.allocMsgId();
    std::shared_ptr<SrsCcApiMediaMsgVideoFrame> vframe = std::make_shared<SrsCcApiMediaMsgVideoFrame>(msgId, streamId);
    if(is_sequence_header) {
        vframe->vframeType = _e_vConfigFrame;
    }else if(srcInfoPtr->_rtmp_format->video->has_idr) {
        vframe->vframeType = _e_vIFrame;
    }
    vframe->codecId = srcInfoPtr->_rtmp_format->vcodec->id;
    vframe->dts = srcInfoPtr->_rtmp_format->video->dts;
    vframe->cts = srcInfoPtr->_rtmp_format->video->cts;
    if(is_sequence_header) {
        std::vector<char>& spsNaluBuff = srcInfoPtr->_rtmp_format->vcodec->sequenceParameterSetNALUnit;
        std::vector<char>& ppsNaluBuff = srcInfoPtr->_rtmp_format->vcodec->pictureParameterSetNALUnit;
        vframe->naluStrList.push_back(std::string(spsNaluBuff.begin(), spsNaluBuff.end()));
        vframe->naluStrList.push_back(std::string(ppsNaluBuff.begin(), ppsNaluBuff.end()));
    }else{
        for(int i = 0; i < srcInfoPtr->_rtmp_format->video->nb_samples; ++i) {
            SrsSample* sample = &(srcInfoPtr->_rtmp_format->video->samples[i]);
            if(sample->parse_bframe() != srs_success) {
                continue;
            }
            if(vframe->vframeType == _e_vUnknownFrame) {
                if(sample->bframe) {
                    vframe->vframeType = _e_vBFrame;
                }else{
                    vframe->vframeType = _e_vPFrame;
                }
            }
            vframe->naluStrList.push_back(std::string(sample->bytes, sample->size));
        }
    }
    srs_trace("xxxxxxxxxxxxx toPassRtmpVideoFrame 006 %s, %d %ld", streamId.c_str(), srcInfoPtr->_rtmp_format->video->nb_samples, vframe->naluStrList.size());
    if(vframe->naluStrList.empty()) {
        return;
    }
    gSrsCcApiImplWorker.postMsg(vframe);
}

void SrsCcApiByPasserMgr::toPassRtmpAudioFrame(const std::string& streamId, SrsSharedPtrMessage* frameMsg) {
    if(!gSrsCcApiImplWorker.ison() || streamId == "" || frameMsg == nullptr) {
        return;
    }
    long msgId = 0;
}

std::shared_ptr<SrsCcApiInnerStreamSourceInfo> SrsCcApiByPasserMgr::touchStreamSourceInfo(const std::string& streamId, bool toPass) {
    std::shared_ptr<SrsCcApiInnerStreamSourceInfo>& srcInfoPtr = mStreamSourceMapToPass[streamId];
    if(!srcInfoPtr) {
        srcInfoPtr = std::make_shared<SrsCcApiInnerStreamSourceInfo>();
    }
    return srcInfoPtr;
}
