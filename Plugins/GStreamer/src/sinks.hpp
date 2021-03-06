#pragma once
#include "GStreamerExport.hpp"
#include "MetaObject/object/MetaObject.hpp"
#include "gstreamer.hpp"
#include "Aquila/framegrabbers/IFrameGrabber.hpp"

namespace aq
{
    namespace nodes
    {
        class GStreamer_EXPORT tcpserver: public gstreamer_sink_base
        {
            bool _initialized;
        public:
            enum {
                None = -1
            };
            MO_DERIVE(tcpserver, gstreamer_sink_base)
                ENUM_PARAM(encoders, None);
                ENUM_PARAM(interfaces, None);
                INPUT(SyncedMemory, image, nullptr);
            MO_END;
            tcpserver();
            ~tcpserver();
            virtual void nodeInit(bool firstInit);
            bool processImpl();
        };

        class GStreamer_EXPORT BufferedHeartbeatRtsp : public IGrabber, public gstreamer_src_base
        {
        public:
            virtual void nodeInit(bool firstInit);
        protected:
        };

        class GStreamer_EXPORT JPEGSink: public Node, public gstreamer_src_base
        {
        public:
            JPEGSink();
            MO_DERIVE(JPEGSink, Node)
                PARAM(std::string, gstreamer_pipeline, "");
                SOURCE(cv::Mat, jpeg_buffer, cv::Mat());
                SOURCE(aq::SyncedMemory, decoded, {});
            MO_END;
        protected:
            bool processImpl();
            virtual GstFlowReturn on_pull();
            cv::Mat decode_buffer;
            std::shared_ptr<mo::Context> gstreamer_context;
        };
    }
}
