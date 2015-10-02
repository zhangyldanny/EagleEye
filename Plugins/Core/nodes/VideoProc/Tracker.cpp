#include "nodes/VideoProc/Tracker.h"
#include "nodes/VideoProc/Tracking.hpp"
#include "opencv2/imgproc.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/cudawarping.hpp>
#include <opencv2/cudaimgproc.hpp>
using namespace EagleLib;
NODE_DEFAULT_CONSTRUCTOR_IMPL(KeyFrameTracker)
NODE_DEFAULT_CONSTRUCTOR_IMPL(CMTTracker)
NODE_DEFAULT_CONSTRUCTOR_IMPL(TLDTracker)
#ifdef _MSC_VER


#else
RUNTIME_COMPILER_LINKLIBRARY("-lopencv_cudawarping")
RUNTIME_COMPILER_LINKLIBRARY("-lopencv_highgui")

#endif

void KeyFrameTracker::Init(bool firstInit)
{
    if(firstInit)
    {
        updateParameter("Number of key frames to track", int(5));   // 0
        updateParameter("Tracking Upper qualty", double(0.7));      // 1
        updateParameter("Tracking lower quality", double(0.4));     // 2
        updateParameter("Min key points", int(200));                // 3
        addInputParameter<DetectAndComputeFunctor>("Detector");     // 4
        addInputParameter<TrackSparseFunctor>("Tracker");
        addInputParameter<cv::cuda::GpuMat>("Mask");
        addInputParameter<int>("Index");
        addInputParameter<boost::function<void(cv::cuda::GpuMat, cv::cuda::GpuMat,
                                               cv::cuda::GpuMat, cv::cuda::GpuMat,
                                               std::string&, cv::cuda::Stream)>>("Display functor");

    }
    updateParameter("Display", true);
    homographyBuffer.resize(20, nullptr);
    trackedFrames.set_capacity(5);
    warpedImageBuffer.resize(20);
}
void KeyFrameTracker::reset()
{
	trackedFrames.clear();
}

void KeyFrameTracker_findHomographyCallback(int status, void* userData)
{

    TrackingResults* results = (TrackingResults*)userData;
    boost::mutex::scoped_lock(results->mtx);
    cv::Mat mask, refPts, trackedPts;
    cv::Mat finalMask = results->h_status.createMatHeader();
    // Pre filter points
    std::vector<int> idxMap;
    if(results->preFilter)
    {
        int goodPts = cv::countNonZero(results->h_status);
        idxMap.reserve(goodPts);
        refPts = cv::Mat(goodPts, 1, CV_32FC2);
        trackedPts = cv::Mat(goodPts, 1, CV_32FC2);
        uchar* maskPtr = finalMask.ptr<uchar>(0);
        cv::Mat refPts_ = results->h_keyFramePts.createMatHeader();
        cv::Mat trackedPts_ = results->h_trackedFramePts.createMatHeader();
        int itr = 0; // Insertion iterator
        for(int i = 0; i < results->h_keyFramePts.cols; ++i, ++maskPtr)
        {
            if(*maskPtr)
            {
               trackedPts.at<cv::Vec2f>(itr) = trackedPts_.at<cv::Vec2f>(i);
                refPts.at<cv::Vec2f>(itr) = refPts_.at<cv::Vec2f>(i);
                idxMap.push_back(i);
                ++itr;
            }
        }
    }else
    {
        refPts = results->h_keyFramePts.createMatHeader();
        trackedPts = results->h_trackedFramePts.createMatHeader();
    }
    results->homography = cv::findHomography(refPts, trackedPts, cv::RANSAC, 3, mask, 2000, 0.995);
    // Post filter based on ransac inliers

    finalMask = cv::Scalar(0);
    int count = 0;
    for(int i = 0; i < mask.rows; ++i)
    {
        if(idxMap.size())
        {
            if(mask.at<uchar>(i))
            {
                finalMask.at<uchar>(idxMap[i]) = 255;
                ++count;
            }
        }else
        {
            finalMask.at<uchar>(i) = 255;
            ++count;
        }
    }
    results->cv.notify_one();
    results->quality = float(count) / float(mask.rows);
    std::cout << results->quality << std::endl;
    results->calculated = true;
}
void displayCallback(cv::cuda::GpuMat img, std::string name)
{
    cv::namedWindow(name, cv::WINDOW_OPENGL);
    cv::imshow(name, img);
}

void KeyFrameTracker_displayCallback(int status, void* userData)
{
    std::pair<cv::cuda::GpuMat*, std::string>* data = (std::pair<cv::cuda::GpuMat*, std::string>*)userData;
    boost::function<void(void)> f = boost::bind(displayCallback, *data->first, data->second);
	Parameters::UI::UiCallbackService::Instance()->post(f);
    //UIThreadCallback::getInstance().addCallback(f);
    delete data;
}

cv::cuda::GpuMat KeyFrameTracker::doProcess(cv::cuda::GpuMat &img, cv::cuda::Stream& stream)
{

    if(parameters[0]->changed)
    {
        trackedFrames.set_capacity(*getParameter<int>(0)->Data());
        parameters[0]->changed = false;
    }
    DetectAndComputeFunctor* detector = getParameter<DetectAndComputeFunctor>("Detector")->Data();
    TrackSparseFunctor* tracker = getParameter<TrackSparseFunctor>("Tracker")->Data();
    cv::cuda::GpuMat* mask = getParameter<cv::cuda::GpuMat>("Mask")->Data();
//    boost::function<void(cv::cuda::GpuMat, cv::cuda::GpuMat,
//                         cv::cuda::GpuMat, cv::cuda::GpuMat,
//                         std::string&, cv::cuda::Stream)>*
//        display = getParameter<boost::function<void(cv::cuda::GpuMat, cv::cuda::GpuMat,
//                                                    cv::cuda::GpuMat, cv::cuda::GpuMat,
//                                                    std::string&, cv::cuda::Stream)>*>("Display functor")->data;
    if(*getParameter<bool>("Display")->Data())
    {
        if(nonWarpedMask.size() != img.size())
        {
            nonWarpedMask = cv::cuda::GpuMat(img.size(), CV_32F);
            nonWarpedMask.setTo(cv::Scalar(1), stream);
        }
    }
	int* index = getParameter<int>("Index")->Data();
    static int frameCount = 0;
    if(index)
        frameCount = *index;
    bool addKeyFrame = false;
    if(!detector || !tracker)
        return img;
    if(trackedFrames.empty())
    {
        addKeyFrame = true;

    }else
    {
        // Track this frame relative to all of the tracked frames
        static std::vector<cv::cuda::Stream> workStreams;
        static cv::cuda::Event startEvent;
        static std::vector<cv::cuda::Event> workFinishedEvents;
        startEvent.record(stream);
        workStreams.resize(trackedFrames.size());
        workFinishedEvents.resize(trackedFrames.size());

        size_t i = 0;
        std::vector<TrackingResults*> results;
        for(auto itr = trackedFrames.begin(); itr != trackedFrames.end(); ++itr, ++i)
        {
            workStreams[i].waitEvent(startEvent);

            (*tracker)(itr->keyFrame.img, img,
                       itr->keyFrame.getKeyPoints(),
                       itr->trackedPoints,
                       itr->status,
                       itr->error, workStreams[i]);

            TrackingResults** tmp = homographyBuffer.getFront();
            if(*tmp == nullptr)
                *tmp = new TrackingResults();
            TrackingResults* h_buffer = *tmp;
            h_buffer->KeyFrameIdx = itr->keyFrame.frameIndex;
            h_buffer->TrackedFrameIdx = frameCount;
            h_buffer->d_keyFramePts = itr->keyFrame.getKeyPoints();
            h_buffer->d_status = itr->status;
            h_buffer->d_trackedFramePts = itr->trackedPoints;
            h_buffer->d_keyFramePts.download(h_buffer->h_keyFramePts, workStreams[i]);
            h_buffer->d_status.download(h_buffer->h_status, workStreams[i]);
            h_buffer->d_trackedFramePts.download(h_buffer->h_trackedFramePts, workStreams[i]);
            h_buffer->preFilter = true;
            h_buffer->calculated = false;

            workStreams[i].enqueueHostCallback(KeyFrameTracker_findHomographyCallback, h_buffer);
            results.push_back(h_buffer);
        }
        i = 0;
        for(auto itr = trackedFrames.begin(); itr != trackedFrames.end(); ++itr, ++i)
        {
            boost::mutex::scoped_lock lock(results[i]->mtx);
            if(results[i]->calculated == false)
            {
                auto start = clock();
                results[i]->cv.wait(lock);
                std::cout << "Waited for: " << clock() - start << " cycles" << std::endl;
            }
            if(results[i]->calculated)
            {
				if (results[i]->quality < *getParameter<double>(2)->Data())
                    addKeyFrame = true;
				if (i == trackedFrames.size() - 1 && results[i]->quality <*getParameter<double>(1)->Data())
                    addKeyFrame = true;
            }
			if (*getParameter<bool>("Display")->Data() == true)
            {
                cv::cuda::GpuMat* warpBuffer = warpedImageBuffer.getFront();
                cv::cuda::GpuMat* maskBuffer = warpedMaskBuffer.getFront();

                cv::cuda::warpPerspective(img,
                    *warpBuffer,
                    results[i]->homography,
                    img.size(), cv::INTER_CUBIC,
                    cv::BORDER_REPLICATE, cv::Scalar(), workStreams[i]);

                cv::cuda::warpPerspective(nonWarpedMask,
                    *maskBuffer,
                    results[i]->homography,
                    img.size(), cv::INTER_CUBIC,
                    cv::BORDER_CONSTANT, cv::Scalar(0), workStreams[i]);
                cv::cuda::GpuMat* d_disp = d_displayBuffer.getFront();
                cv::cuda::blendLinear(img, *warpBuffer, nonWarpedMask, *maskBuffer, *d_disp, workStreams[i]);
                workStreams[i].enqueueHostCallback(KeyFrameTracker_displayCallback,
                    new std::pair<cv::cuda::GpuMat*, std::string>(d_disp, "Warped image " + boost::lexical_cast<std::string>(i)));
            }
        }
    }
    if(addKeyFrame)
    {
        //log(Status, "Adding key frame " + boost::lexical_cast<std::string>(frameCount));
		NODE_LOG(info) << "Adding key frame " + frameCount;
        TrackedFrame tf(img,frameCount);
        cv::cuda::GpuMat& keyPoints = tf.keyFrame.getKeyPoints();
        (*detector)(img,
                    mask? *mask: cv::cuda::GpuMat(),
                    keyPoints,
                    tf.keyFrame.getDescriptors(),
                    stream);
		if (keyPoints.cols > *getParameter<int>(3)->Data())
            trackedFrames.push_back(tf);
    }
    ++frameCount;
    return img;
}

void CMTTracker::Init(bool firstInit)
{

}

cv::cuda::GpuMat CMTTracker::doProcess(cv::cuda::GpuMat &img, cv::cuda::Stream& stream)
{
    return img;
}

void TLDTracker::Init(bool firstInit)
{

}

cv::cuda::GpuMat TLDTracker::doProcess(cv::cuda::GpuMat &img, cv::cuda::Stream& stream)
{
    return img;
}