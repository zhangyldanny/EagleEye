#include "nodes/IO/Camera.h"

using namespace EagleLib;

bool Camera::changeStream(int device)
{
    try
    {
        log(Status, "Setting camera to device: " + boost::lexical_cast<std::string>(device));
        cam.release();
        cam = cv::VideoCapture(device);
        return cam.isOpened();
    }catch(cv::Exception &e)
    {
        log(Error, e.what());
        return false;
    }

}
bool Camera::changeStream(const std::string &gstreamParams)
{
    try
    {
        log(Status, "Setting camera with gstreamer settings: " + gstreamParams);
        cam.release();
        cam = cv::VideoCapture(gstreamParams);
        return cam.isOpened();
    }catch(cv::Exception &e)
    {
        log(Error, e.what());
        return false;
    }

}
Camera::~Camera()
{
//    acquisitionThread.interrupt();
//    acquisitionThread.join();
}

void Camera::Init(bool firstInit)
{
    Node::Init(firstInit);
    if(firstInit)
    {
        updateParameter<int>("Camera Number", 0);
        updateParameter<std::string>("Gstreamer stream ", "");
        parameters[0]->changed = false;
        parameters[1]->changed = false;
    }
}
void Camera::Serialize(ISimpleSerializer *pSerializer)
{
    Node::Serialize(pSerializer);
    SERIALIZE(cam);
}
//void Camera::acquisitionLoop()
//{
//    cv::cuda::HostMem buffer;
//    Buffer<cv::cuda::GpuMat, EventPolicy, LockedPolicy>* d_buf;
//    while(!boost::this_thread::interruption_requested())
//    {
//        d_buf = imageBuffer.waitFront();
//        if(d_buf)
//        {
//            boost::recursive_mutex::scoped_lock lock(d_buf->mtx);
//            if(cam.isOpened())
//            {
//                cam.read(buffer);
//                d_buf->data.upload(buffer);
//            }
//        }
//    }
//}

cv::cuda::GpuMat Camera::doProcess(cv::cuda::GpuMat &img, cv::cuda::Stream& stream)
{
    if(parameters[0]->changed)
    {
        parameters[0]->changed = false;
        changeStream(getParameter<int>(0)->data);
    }
    if(parameters[1]->changed)
    {
        parameters[1]->changed = false;
        changeStream(getParameter<std::string>(1)->data);
    }
    if(cam.isOpened())
    {
        cam.read(hostBuf);
        img.upload(hostBuf,stream);
    }
    updateParameter("Output", img, Parameter::Output);
    return img;
}
bool Camera::SkipEmpty() const
{
    return false;
}
void GStreamerCamera::Init(bool firstInit)
{
    Node::Init(firstInit);
    if(firstInit)
    {
        EnumParameter param;
        param.addEnum(ENUM(v4l2src));
        EnumParameter type;
        type.addEnum(ENUM(h264));
        updateParameter("Source type", param);
        updateParameter("Source encoding", type);
        updateParameter<std::string>("Source", "/dev/video0");
        updateParameter("Width", int(1920));
        updateParameter("Height", int(1080));
        updateParameter<std::string>("Framerate", "30/1");
        updateParameter("Queue", true);
        setString();
        //updateParameter("Gstreamer string", "v4l2src device=/dev/video0 ! video/x-h264, width=1920, height=1080, framerate=30/1 ! queue ! h264parse ! avdec_h264 ! videoconvert ! video/x-raw, width=1920, height=1080 ! appsink");
    }
}
void GStreamerCamera::setString()
{
    std::stringstream str;
    switch(getParameter<EnumParameter>(0)->data.getValue())
    {
        case v4l2src:
        str << "v4l2src ";
        break;
    }

    str << "device=" << getParameter<std::string>(2)->data << " ! ";
    VideoType encoding = (VideoType)getParameter<EnumParameter>(1)->data.getValue();
    if(encoding == h264)
        str << "video/x-h264, width=";
    str << getParameter<int>(3)->data;
    str << ", height=";
    str << getParameter<int>(4)->data;
    str << ", framerate=";
    str << getParameter<std::string>(5)->data;
    str << " ! ";
    if(getParameter<bool>(6)->data)
        str << "queue ! ";
    if(encoding == h264)
        str << "h264parse ! avdec_h264 ! videoconvert ! video/x-raw, width=";
    str << getParameter<int>(3)->data;
    str << ", height=";
    str << getParameter<int>(4)->data;
    str << " ! appsink";
    std::string result = str.str();
    // "v4l2src device=/dev/video0 ! video/x-h264, width=1920, height=1080, framerate=30/1 ! queue ! h264parse ! avdec_h264 ! videoconvert ! video/x-raw, width=1920, height=1080 ! appsink"
    updateParameter<std::string>("Gstreamer string", result);
    cam.release();
    try
    {
        cam.open(result);
    }catch(cv::Exception &e)
    {
        log(Error, e.what());
        return;
    }

    if(cam.isOpened())
        log(Status, "Successfully opened camera");
    else
        log(Error, "Failed to open camera");

    for(size_t i = 0; i < parameters.size(); ++i)
    {
        parameters[i]->changed = false;
    }
}

cv::cuda::GpuMat GStreamerCamera::doProcess(cv::cuda::GpuMat &img, cv::cuda::Stream &stream)
{
    if(parameters[0]->changed ||
        parameters[1]->changed ||
        parameters[2]->changed ||
        parameters[3]->changed ||
        parameters[4]->changed ||
        parameters[5]->changed ||
        parameters[6]->changed ||
        parameters[7]->changed)
    {
        setString();
    }
    if(cam.isOpened())
    {
        if(cam.read(hostBuf))
        {
            img.upload(hostBuf,stream);
        }

    }
    updateParameter("Output", img, Parameter::Output);
    return img;
}
bool GStreamerCamera::SkipEmpty() const
{
    return false;
}

NODE_DEFAULT_CONSTRUCTOR_IMPL(Camera)
NODE_DEFAULT_CONSTRUCTOR_IMPL(GStreamerCamera)