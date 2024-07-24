#include "srs_app_zz_ccapi_bypasser_mgr.hpp"
#include "srs_app_zz_ccapi_impl.hpp"
#include "srs_kernel_codec.hpp"
#include "srs_protocol_format.hpp"
#include "srs_protocol_rtmp_stack.hpp"
#include "srs_protocol_raw_avc.hpp"
#include "srs_kernel_buffer.hpp"
#include "srs_kernel_log.hpp"

class SrsCcApiInnerStreamSourceInfo
{
public:
    friend class SrsCcApiByPasserMgr;
    SrsCcApiInnerStreamSourceInfo() {

    }
    virtual ~SrsCcApiInnerStreamSourceInfo() {

    }

private:
    //for toPass mode
    SrsRtmpFormat _rtmp_format;

private:
    //for onPass mode
    std::map<std::string, uint8_t> _audioFormatConfig;
};

static SrsRequest genSrsRequestByStreamId(const std::string& stream_id) {
    SrsRequest srsReq;
    srsReq.param = "zz_ccapi_onpass=1";
    srsReq.vhost = SRS_CONSTS_RTMP_DEFAULT_VHOST;
    srsReq.app = "";
    srsReq.stream = "";
    size_t posL = 0;
    //vhost
    size_t posR = stream_id.find("/", posL);
    if(posR == std::string::npos) {
        return srsReq;
    }
    if(posR > posL) {
        srsReq.vhost = stream_id.substr(posL, posR-posL);
    }
    posL = posR+1;
    //app
    posR = stream_id.find("/", posL);
    if(posR == std::string::npos) {
        return srsReq;
    }
    if(posR > posL) {
        srsReq.app = stream_id.substr(posL, posR-posL);
    }
    posL = posR+1;
    //stream
    posR = stream_id.size();
    if(posR > posL) {
        srsReq.stream = stream_id.substr(posL, posR-posL);
    }
    return srsReq;
}

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
    if(srcInfoPtr->_rtmp_format.on_video(frameMsg) != srs_success) {
        return;
    }
    srs_trace("xxxxxxxxxxxxx toPassRtmpVideoFrame 004 %s", streamId.c_str());
    if(!srcInfoPtr->_rtmp_format.vcodec || !srcInfoPtr->_rtmp_format.video) {
        return;
    }
    srs_trace("xxxxxxxxxxxxx toPassRtmpVideoFrame 005 %s", streamId.c_str());
    long msgId = gSrsCcApiImplWorker.allocMsgId();
    std::shared_ptr<SrsCcApiMediaMsgVideoFrame> vframe = std::make_shared<SrsCcApiMediaMsgVideoFrame>(msgId, streamId);
    if(is_sequence_header) {
        vframe->vframeType = SrsCcApiMediaMsgVideoFrame::_e_vConfigFrame;
    }else if(srcInfoPtr->_rtmp_format.video->has_idr) {
        vframe->vframeType = SrsCcApiMediaMsgVideoFrame::_e_vIFrame;
    }
    vframe->codecId = srcInfoPtr->_rtmp_format.vcodec->id;
    vframe->dts = srcInfoPtr->_rtmp_format.video->dts;
    vframe->cts = srcInfoPtr->_rtmp_format.video->cts;
    if(is_sequence_header) {
        if(vframe->codecId == 7) { //h264
            std::vector<char>& spsNaluBuff = srcInfoPtr->_rtmp_format.vcodec->sequenceParameterSetNALUnit;
            std::vector<char>& ppsNaluBuff = srcInfoPtr->_rtmp_format.vcodec->pictureParameterSetNALUnit;
            vframe->naluStrList.push_back(std::string(spsNaluBuff.begin(), spsNaluBuff.end()));
            vframe->naluStrList.push_back(std::string(ppsNaluBuff.begin(), ppsNaluBuff.end()));
        }else if(vframe->codecId == 12) { //h265
            for(size_t i = 0; i < srcInfoPtr->_rtmp_format.vcodec->hevc_dec_conf_record_.nalu_vec.size(); ++i) {                
                SrsHevcHvccNalu& hecv_hvcc_nalu = srcInfoPtr->_rtmp_format.vcodec->hevc_dec_conf_record_.nalu_vec[i];
                for(size_t j = 0; j < hecv_hvcc_nalu.nal_data_vec.size(); ++j) {
                    SrsHevcNalData& hevcNaluData = hecv_hvcc_nalu.nal_data_vec[j];
                    vframe->naluStrList.push_back(std::string(hevcNaluData.nal_unit_data.begin(), hevcNaluData.nal_unit_data.end()));
                }
            }
        }
    }else{
        for(int i = 0; i < srcInfoPtr->_rtmp_format.video->nb_samples; ++i) {
            SrsSample* sample = &(srcInfoPtr->_rtmp_format.video->samples[i]);
            if(sample->parse_bframe() != srs_success) {
                continue;
            }
            if(vframe->vframeType == SrsCcApiMediaMsgVideoFrame::_e_vUnknownFrame) {
                if(sample->bframe) {
                    vframe->vframeType = SrsCcApiMediaMsgVideoFrame::_e_vBFrame;
                }else{
                    vframe->vframeType = SrsCcApiMediaMsgVideoFrame::_e_vPFrame;
                }
            }
            vframe->naluStrList.push_back(std::string(sample->bytes, sample->size));
        }
    }
    srs_trace("xxxxxxxxxxxxx toPassRtmpVideoFrame 006 %s, %d %ld", streamId.c_str(), srcInfoPtr->_rtmp_format.video->nb_samples, vframe->naluStrList.size());
    if(vframe->naluStrList.empty()) {
        return;
    }
    gSrsCcApiImplWorker.postMsg(vframe);
}

void SrsCcApiByPasserMgr::toPassRtmpAudioFrame(const std::string& streamId, SrsSharedPtrMessage* frameMsg) {
    srs_trace("xxxxxxxxxxxxx toPassRtmpAudioFrame 001 %s", streamId.c_str());
    if(!gSrsCcApiImplWorker.ison() || streamId == "" || frameMsg == nullptr) {
        return;
    }
    srs_trace("xxxxxxxxxxxxx toPassRtmpAudioFrame 002 %s", streamId.c_str());
    std::shared_ptr<SrsCcApiInnerStreamSourceInfo> srcInfoPtr = touchStreamSourceInfo(streamId, true);
    if(!srcInfoPtr) {
        return;
    }
    srs_trace("xxxxxxxxxxxxx toPassRtmpAudioFrame 003 %s", streamId.c_str());
    if(srcInfoPtr->_rtmp_format.on_audio(frameMsg) != srs_success) {
        return;
    }
    srs_trace("xxxxxxxxxxxxx toPassRtmpAudioFrame 004 %s", streamId.c_str());
    if(!srcInfoPtr->_rtmp_format.acodec || !srcInfoPtr->_rtmp_format.audio) {
        return;
    }
    bool is_sequence_header = srcInfoPtr->_rtmp_format.is_aac_sequence_header();
    srs_trace("xxxxxxxxxxxxx toPassRtmpAudioFrame 005 %s", streamId.c_str());
    long msgId = gSrsCcApiImplWorker.allocMsgId();
    std::shared_ptr<SrsCcApiMediaMsgAudioFrame> aframe = std::make_shared<SrsCcApiMediaMsgAudioFrame>(msgId, streamId);
    if(is_sequence_header) {
        aframe->aframeType = SrsCcApiMediaMsgAudioFrame::_e_aConfigFrame;
    }else{
        aframe->aframeType = SrsCcApiMediaMsgAudioFrame::_e_aIFrame;
    }
    aframe->codecId = srcInfoPtr->_rtmp_format.acodec->id;
    aframe->dts = srcInfoPtr->_rtmp_format.audio->dts;
    if(is_sequence_header) {
        std::vector<char>& audioSpecificConfigDataBuff = srcInfoPtr->_rtmp_format.acodec->aac_extra_data;
        aframe->rawStr = std::string(audioSpecificConfigDataBuff.begin(), audioSpecificConfigDataBuff.end());
    }else{
        if(srcInfoPtr->_rtmp_format.audio->nb_samples == 1) {
            SrsSample* sample = &(srcInfoPtr->_rtmp_format.audio->samples[0]);
            aframe->rawStr = std::string(sample->bytes, sample->size);
        }
    }
    aframe->extraMap["sampleBitFlag"] = std::to_string((int)srcInfoPtr->_rtmp_format.acodec->sound_size);
    srs_trace("xxxxxxxxxxxxx toPassRtmpAudioFrame 006 %s, %ld %ld", streamId.c_str(), srcInfoPtr->_rtmp_format.audio->nb_samples, aframe->rawStr.size());
    if(aframe->rawStr == "") {
        return;
    }
    gSrsCcApiImplWorker.postMsg(aframe);
}

void SrsCcApiByPasserMgr::onPassVideoFrame(SrsCcApiMediaMsgVideoFrame* videoFrame) {
    if(!gSrsCcApiImplWorker.ison() || videoFrame == nullptr || videoFrame->_stream_id == "") {
        return;
    }
    std::shared_ptr<SrsCcApiInnerStreamSourceInfo> srcInfoPtr = touchStreamSourceInfo(videoFrame->_stream_id, false);
    if(!srcInfoPtr) {
        return;
    }
    SrsRequest req = genSrsRequestByStreamId(videoFrame->_stream_id);
    SrsLiveSource* liveSource = _srs_sources->fetch(&req);
    if(liveSource == nullptr) {
        _srs_sources->fetch_or_create(&req, this,  &liveSource);
        if(liveSource) {
            liveSource->on_publish();
        }
    }
    if(liveSource == nullptr) {
        return;
    }
    SrsCommonMessage frameMsg;
    std::string _videoSequenceHeaderStr = "";
    uint32_t payloadLen = 0;
    if(videoFrame->vframeType == SrsCcApiMediaMsgVideoFrame::__em_vframe_type::_e_vConfigFrame) {
        std::string sh = "";
        if(videoFrame->codecId == 7) {
            SrsRawH264Stream avc;
            std::string sps = "";
            std::string pps = "";
            for(auto& lit : videoFrame->naluStrList) {
                if(lit.size() == 0) {
                    continue;
                }
                if(sps == "" && avc.is_sps((char*)lit.data(), lit.size())) {
                    sps = lit;
                }
                if(pps == "" && avc.is_pps((char*)lit.data(), lit.size())) {
                    pps = lit;
                }
                if(sps != "" && pps != "") {
                    break;
                }
            }
            avc.mux_sequence_header(sps, pps, sh);
        }else if(videoFrame->codecId == 12) {
            SrsRawHEVCStream hevc;
            std::string sps = "";
            std::string pps = "";
            std::vector<std::string> vps;
            for(auto& lit : videoFrame->naluStrList) {
                if(lit.size() == 0) {
                    continue;
                }
                if(sps == "" && hevc.is_sps((char*)lit.data(), lit.size())) {
                    sps = lit;
                }
                if(pps == "" && hevc.is_pps((char*)lit.data(), lit.size())) {
                    pps = lit;
                }
                if(hevc.is_vps((char*)lit.data(), lit.size())) {
                    vps.push_back(lit);
                }
            }
            hevc.mux_sequence_header(sps, pps, vps, sh);
        }
        if(sh != "") {
            std::swap(_videoSequenceHeaderStr, sh);
        }
        payloadLen = 5 + _videoSequenceHeaderStr.size();
    }else{
        payloadLen = 5;
        for(auto& lit : videoFrame->naluStrList) {
            payloadLen += 4;
            payloadLen += lit.size();
        }
    }
    frameMsg.header.initialize_video(payloadLen, videoFrame->dts, 1);
    frameMsg.create_payload(payloadLen);
    frameMsg.size = payloadLen;
    SrsBuffer payload(frameMsg.payload, frameMsg.size);
    uint8_t video_format = 0;
    if(videoFrame->vframeType == SrsCcApiMediaMsgVideoFrame::__em_vframe_type::_e_vConfigFrame || videoFrame->vframeType == SrsCcApiMediaMsgVideoFrame::__em_vframe_type::_e_vIFrame) {
        video_format |= 0x10;;
    }else{
        video_format |= 0x20;
    }
    video_format |= (((uint8_t)videoFrame->codecId) & 0x0f);
    payload.write_1bytes(video_format);
    if(videoFrame->vframeType == SrsCcApiMediaMsgVideoFrame::__em_vframe_type::_e_vConfigFrame) {
        payload.write_1bytes(0);
    }else{
        payload.write_1bytes(1);
    }
    payload.write_1bytes((uint8_t)(videoFrame->cts >> 2));
    payload.write_1bytes((uint8_t)(videoFrame->cts >> 1));
    payload.write_1bytes((uint8_t)(videoFrame->cts));
    if(videoFrame->vframeType == SrsCcApiMediaMsgVideoFrame::__em_vframe_type::_e_vConfigFrame) {
        if(_videoSequenceHeaderStr.size() > 0) {
            payload.write_bytes((char*)_videoSequenceHeaderStr.data(), _videoSequenceHeaderStr.size());
        }
    }else{
        for(auto& lit : videoFrame->naluStrList) {
            payload.write_4bytes(lit.size());
            if(lit.size() > 0) {
                payload.write_bytes((char*)lit.data(), lit.size());
            }
        }
    }
    liveSource->on_video(&frameMsg);
}

void SrsCcApiByPasserMgr::onPassAudioFrame(SrsCcApiMediaMsgAudioFrame* audioFrame) {
    if(!gSrsCcApiImplWorker.ison() || audioFrame == nullptr || audioFrame->_stream_id == "") {
        return;
    }
    std::shared_ptr<SrsCcApiInnerStreamSourceInfo> srcInfoPtr = touchStreamSourceInfo(audioFrame->_stream_id, false);
    if(!srcInfoPtr) {
        return;
    }
    SrsRequest req = genSrsRequestByStreamId(audioFrame->_stream_id);
    SrsLiveSource* liveSource = _srs_sources->fetch(&req);
    if(liveSource == nullptr) {
        _srs_sources->fetch_or_create(&req, this, &liveSource);
        if(liveSource) {
            liveSource->on_publish();
        }
    }
    if(liveSource == nullptr) {
        return;
    }
    SrsCommonMessage frameMsg;
    uint32_t payloadLen = 2 + audioFrame->rawStr.size();
    if(audioFrame->aframeType == SrsCcApiMediaMsgAudioFrame::__em_aframe_type::_e_aConfigFrame) {
        if(audioFrame->codecId == 10) {
            SrsFormat aacFmt;
            aacFmt.initialize();
            if(srs_success == aacFmt.on_aac_sequence_header((char*)audioFrame->rawStr.data(), audioFrame->rawStr.size())) {
                srcInfoPtr->_audioFormatConfig["soundRateIndex"] = aacFmt.acodec->aac_sample_rate;
                srcInfoPtr->_audioFormatConfig["soundChannelFlag"] = aacFmt.acodec->aac_channels;
                srcInfoPtr->_audioFormatConfig["soundBitFlag"] = (uint8_t)atoi(audioFrame->extraMap["sampleBitFlag"].c_str());
            }
        }
    }
    frameMsg.header.initialize_audio(payloadLen, audioFrame->dts, 1);
    frameMsg.create_payload(payloadLen);
    SrsBuffer stream(frameMsg.payload, payloadLen);
    uint8_t audio_format = 0;
    audio_format |= ((((uint8_t)audioFrame->codecId) & 0x0f) << 4);
    int s_frequency[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350 };
    int s_index = srcInfoPtr->_audioFormatConfig["soundRateIndex"];
    if(s_index >= 0 && s_index < 13) {
        if(s_frequency[s_index] >= 44100) {
            audio_format |= ((((uint8_t)(3)) & 0x03) << 2);
        }else if(s_frequency[s_index] >= 22050) {
            audio_format |= ((((uint8_t)(2)) & 0x03) << 2);
        }else if(s_frequency[s_index] >= 11025) {
            audio_format |= ((((uint8_t)(1)) & 0x03) << 2);
        }else{
            audio_format |= ((((uint8_t)(0)) & 0x03) << 2);
        }
    }else{
        audio_format |= ((((uint8_t)(3)) & 0x03) << 2);
    }
    if(srcInfoPtr->_audioFormatConfig["soundBitFlag"] > 0) {
        audio_format |= ((((uint8_t)(1)) & 0x01) << 1);
    }else{
        audio_format |= ((((uint8_t)(0)) & 0x01) << 1);
    }
    if(srcInfoPtr->_audioFormatConfig["soundChannelFlag"] > 1) {
        audio_format |= (((uint8_t)(1)) & 0x01);
    }else{
        audio_format |= (((uint8_t)(0)) & 0x01);
    }
    stream.write_1bytes(audio_format);
    if(audioFrame->aframeType == SrsCcApiMediaMsgAudioFrame::__em_aframe_type::_e_aConfigFrame) {
        stream.write_1bytes(0);
    }else{
        stream.write_1bytes(1);
    }
    stream.write_bytes((char*)audioFrame->rawStr.data(), audioFrame->rawStr.size());
    frameMsg.size = payloadLen;
    liveSource->on_audio(&frameMsg);
}

srs_error_t SrsCcApiByPasserMgr::on_publish(SrsLiveSource* s, SrsRequest* r) {
    srs_error_t err = srs_success;
    if(s != nullptr && r != nullptr) {
        srs_info("SrsCcApiByPasserMgr::on_publish, streamid:%s", r->get_stream_url().c_str());
    }
    return err;
}

void SrsCcApiByPasserMgr::on_unpublish(SrsLiveSource* s, SrsRequest* r) {
    if(s != nullptr && r != nullptr) {
        srs_info("SrsCcApiByPasserMgr::on_unpublish, streamid:%s", r->get_stream_url().c_str());
    }
}

std::shared_ptr<SrsCcApiInnerStreamSourceInfo> SrsCcApiByPasserMgr::touchStreamSourceInfo(const std::string& streamId, bool toPass) {
    if(toPass) {
        std::shared_ptr<SrsCcApiInnerStreamSourceInfo>& srcInfoPtr = mStreamSourceMapToPass[streamId];
        if(!srcInfoPtr) {
            srcInfoPtr = std::make_shared<SrsCcApiInnerStreamSourceInfo>();
        }
        return srcInfoPtr;
    }else{
        std::shared_ptr<SrsCcApiInnerStreamSourceInfo>& srcInfoPtr = mStreamSourceMapOnPass[streamId];
        if(!srcInfoPtr) {
            srcInfoPtr = std::make_shared<SrsCcApiInnerStreamSourceInfo>();
        }
        return srcInfoPtr;
    }
}
