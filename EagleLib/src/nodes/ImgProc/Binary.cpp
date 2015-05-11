#include "nodes/ImgProc/Binary.h"
#include <opencv2/cudaarithm.hpp>
#include <opencv2/cudafilters.hpp>
#include <opencv2/cudabgsegm.hpp>
#include <opencv2/cudafeatures2d.hpp>
#include <opencv2/cudaimgproc.hpp>
#include <opencv2/cudalegacy.hpp>
#include <opencv2/cudaobjdetect.hpp>
#include <algorithm>
#include <utility>
using namespace EagleLib;

void MorphologyFilter::Init(bool firstInit)
{
    if(firstInit)
    {
        EnumParameter structuringElement;
        structuringElement.addEnum(ENUM(cv::MORPH_RECT));
        structuringElement.addEnum(ENUM(cv::MORPH_CROSS));
        structuringElement.addEnum(ENUM(cv::MORPH_ELLIPSE));
        updateParameter("Structuring Element Type", structuringElement);    // 0
        EnumParameter morphType;
        morphType.addEnum(ENUM(cv::MORPH_ERODE));
        morphType.addEnum(ENUM(cv::MORPH_DILATE));
        morphType.addEnum(ENUM(cv::MORPH_OPEN));
        morphType.addEnum(ENUM(cv::MORPH_CLOSE));
        morphType.addEnum(ENUM(cv::MORPH_GRADIENT));
        morphType.addEnum(ENUM(cv::MORPH_TOPHAT));
        morphType.addEnum(ENUM(cv::MORPH_BLACKHAT));
        updateParameter("Morphology type", morphType);  //1
        updateParameter("Structuring Element Size", int(5)); // 2
        updateParameter("Anchor Point", cv::Point(-1,-1));  // 3
        updateParameter("Structuring Element", cv::getStructuringElement(0,cv::Size(5,5))); // 4
        updateParameter("Iterations", int(1));
    }
}

cv::cuda::GpuMat MorphologyFilter::doProcess(cv::cuda::GpuMat &img, cv::cuda::Stream& stream)
{
    bool updateFilter = parameters.size() != 7;
    if(parameters[0]->changed || parameters[2]->changed)
    {
        int size = getParameter<int>(2)->data;
        cv::Point anchor = getParameter<cv::Point>(3)->data;
        updateParameter(4, cv::getStructuringElement(getParameter<EnumParameter>(0)->data.currentSelection,
                                  cv::Size(size,size),anchor));

        updateFilter = true;
        parameters[0]->changed = false;
        parameters[2]->changed = false;
        log(Status,"Structuring element updated");
    }
    if(parameters[1]->changed || updateFilter)
    {
        updateParameter("Filter",
            cv::cuda::createMorphologyFilter(
                getParameter<EnumParameter>(1)->data.currentSelection,img.type(),
                getParameter<cv::Mat>(4)->data,
                getParameter<cv::Point>(3)->data,
                getParameter<int>(5)->data));
        log(Status, "Filter updated");
        parameters[1]->changed = false;
    }
    getParameter<cv::Ptr<cv::cuda::Filter>>(6)->data->apply(img,img,stream);
    return img;
}
void FindContours::Init(bool firstInit)
{
    if(firstInit)
    {
        EnumParameter mode;
        mode.addEnum(ENUM(cv::RETR_EXTERNAL));
        mode.addEnum(ENUM(cv::RETR_LIST));
        mode.addEnum(ENUM(cv::RETR_CCOMP));
        mode.addEnum(ENUM(cv::RETR_TREE));
        mode.addEnum(ENUM(cv::RETR_FLOODFILL));
        EnumParameter method;
        method.addEnum(ENUM(cv::CHAIN_APPROX_NONE));
        method.addEnum(ENUM(cv::CHAIN_APPROX_SIMPLE));
        method.addEnum(ENUM(cv::CHAIN_APPROX_TC89_L1));
        method.addEnum(ENUM(cv::CHAIN_APPROX_TC89_KCOS));
        updateParameter("Mode", mode);      // 0
        updateParameter("Method", method);  // 1
        updateParameter<std::vector<std::vector<cv::Point>>>("Contours", std::vector<std::vector<cv::Point>>(), Parameter::Output); // 2
        updateParameter<std::vector<cv::Vec4i>>("Hierarchy", std::vector<cv::Vec4i>()); // 3
        updateParameter<bool>("Calculate contour Area", false); // 4
        updateParameter<bool>("Calculate Moments", false);  // 5
    }

}

cv::cuda::GpuMat FindContours::doProcess(cv::cuda::GpuMat &img, cv::cuda::Stream& stream)
{
    cv::Mat h_img;
    img.download(h_img, stream);
    stream.waitForCompletion();
    std::vector<std::vector<cv::Point> >* ptr = getParameterPtr<std::vector<std::vector<cv::Point>>>(parameters[2]);
    cv::findContours(h_img,
        *ptr,
        getParameter<std::vector<cv::Vec4i>>(3)->data,
        getParameter<EnumParameter>(0)->data.currentSelection,
        getParameter<EnumParameter>(1)->data.currentSelection);
    updateParameter<int>("Contours found",ptr->size(), Parameter::State);
    parameters[2]->changed = true;
    parameters[3]->changed = true;
    if(getParameter<bool>(4)->data)
    {
        if(parameters[4]->changed)
        {
            updateParameter<std::vector<std::pair<int,double>>>("Contour Area",std::vector<std::pair<int,double>>(), Parameter::Output);
            updateParameter<bool>("Oriented Area",false);
            updateParameter<bool>("Filter area", false);
            parameters[4]->changed = false;
        }
        boost::shared_ptr<TypedParameter<bool>> areaParam = getParameter<bool>("Filter area");
        if(areaParam != nullptr && areaParam->data && areaParam->changed)
        {
            updateParameter<double>("Filter threshold", 0.0);
            updateParameter<double>("Filter sigma", 0.0);
            areaParam->changed = false;
        }
        std::vector<std::pair<int,double>>* areaPtr = getParameterPtr<std::vector<std::pair<int,double>>>(getParameter("Contour Area"));
        bool oriented = getParameter<bool>("Oriented Area")->data;
        areaPtr->resize(ptr->size());
        for(int i = 0; i < ptr->size(); ++i)
        {
            (*areaPtr)[i] = std::pair<int,double>(i,cv::contourArea((*ptr)[i], oriented));
        }
        TypedParameter<double>::Ptr thresholdParam = getParameter<double>("Filter threshold");
        if(thresholdParam != nullptr && thresholdParam->data != 0)
        {
            areaPtr->erase(std::remove_if(areaPtr->begin(), areaPtr->end(),
                            [thresholdParam](std::pair<int,double> x){return x.second < thresholdParam->data;}), areaPtr->end());
            // This should be more efficient, needs to be tested though
            /*for(auto it = areaPtr->begin(); it != areaPtr->end(); ++it)
            {
                if(it->second < thresholdParam->data)
                {
                    std::swap(*it, areaPtr->back());
                    areaPtr->pop_back();
                }
            }*/
        }
        TypedParameter<double>::Ptr sigmaParam = getParameter<double>("Filter sigma");
        if(sigmaParam != nullptr && sigmaParam->data != 0.0)
        {
            // Calculate mean and sigma
            double sum = 0;
            double sumSq = 0;
            for(int i = 0; i < areaPtr->size(); ++i)
            {
                sum += (*areaPtr)[i].second;
                sumSq += (*areaPtr)[i].second*(*areaPtr)[i].second;
            }

        }
    }

    return img;
}
void ContourBoundingBox::Init(bool firstInit)
{
    if(firstInit)
    {
        addInputParameter<std::vector<std::vector<cv::Point>>>("Contours");
        addInputParameter<std::vector<cv::Vec4i>>("Hierarchy");
        addParameter<cv::Scalar>("Box color", cv::Scalar(0,0,255));
        addParameter<int>("Line thickness", 2);
        addInputParameter<std::vector<std::pair<int,double>>>("Contour Area");
        updateParameter<bool>("Use filtered area", false);
    }
    updateParameter<bool>("Merge contours", false);

}

cv::cuda::GpuMat ContourBoundingBox::doProcess(cv::cuda::GpuMat &img, cv::cuda::Stream& stream)
{
    auto contourPtr = getParameter<std::vector<std::vector<cv::Point>>*>(0)->data;
    if(!contourPtr)
        return img;
    std::vector<cv::Rect> boxes;
    for(int i = 0; i < contourPtr->size(); ++i)
    {
        boxes.push_back(cv::boundingRect((*contourPtr)[i]));
    }
    auto mergeParam = getParameter<bool>("Merge contours");
    if(mergeParam && mergeParam->changed)
    {
        updateParameter<int>("Separation distance", 5,Parameter::Control, "Max distance between contours to still merge contours");
    }
    if(mergeParam && mergeParam->data)
    {
        int distance = getParameter<int>("Separation distance")->data;
        for(int i = 0; i < boxes.size() - 1; ++i)
        {
            for(int j = i + 1; j < boxes.size(); ++j)
            {
                // Check distance between bounding rects
                cv::Point c1 = boxes[i].tl() + cv::Point(boxes[i].width/2, boxes[i].height/2);
                cv::Point c2 = boxes[j].tl() + cv::Point(boxes[j].width/2, boxes[j].height/2);
                auto dist = cv::norm(c1 - c2);
                auto thresh = 1.3*(cv::norm(boxes[i].tl() - c1) + cv::norm(boxes[j].tl() - c2));
                if(dist > thresh)
                    continue;

                // If we've made it this far, then we need to merge the rectangles
                cv::Rect newRect = boxes[i] | boxes[j];
                boxes[i] = newRect;
                boxes.erase(boxes.begin() + j);
            }
        }
    }

    cv::Mat h_img;
    img.download(h_img,stream);
    stream.waitForCompletion();
    cv::Scalar replace;
    if(img.channels() == 3)
        replace = getParameter<cv::Scalar>(2)->data;
    else
        replace = cv::Scalar(128,0,0);
    auto useArea = getParameter<bool>("Use filtered area");
    int lineWidth = getParameter<int>(3)->data;
    auto areaParam = getParameter<std::vector<std::pair<int,double>>*>("Contour Area");
    if(useArea && useArea->data && areaParam && areaParam->data)
    {
        for(int i = 0; i < areaParam->data->size(); ++i)
        {
            cv::rectangle(h_img, boxes[(*areaParam->data)[i].first], replace, lineWidth);
        }
    }else
    {
        for(int i = 0; i < boxes.size(); ++i)
        {
            cv::rectangle(h_img, boxes[i],replace, lineWidth);
        }
    }
    img.upload(h_img,stream);
    return img;
}

NODE_DEFAULT_CONSTRUCTOR_IMPL(MorphologyFilter)
NODE_DEFAULT_CONSTRUCTOR_IMPL(FindContours)
NODE_DEFAULT_CONSTRUCTOR_IMPL(ContourBoundingBox)

