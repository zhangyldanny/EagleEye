#include "nodes/SerialStack.h"
using namespace EagleLib;

SerialStack::SerialStack()
{

}
SerialStack::~SerialStack()
{

}
cv::cuda::GpuMat
SerialStack::doProcess(cv::cuda::GpuMat& img)
{
	
	for (auto it = children.begin(); ++it)
	{
		img = it->second->process(img);
	}
    for(int i = 0; i < children.size(); ++i)
    {
        img = children[i]->process(img);
    }
	return img;
}

