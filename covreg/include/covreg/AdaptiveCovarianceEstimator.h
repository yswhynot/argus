#pragma once

#include <ros/ros.h>
#include "argus_utils/filters/FilterInfo.h"
#include "argus_utils/filters/FilterUtils.h"

namespace argus
{

// Based on Mohamed and Schwarz 1999
class AdaptiveCovarianceEstimator
{
public:

	AdaptiveCovarianceEstimator();

	void Initialize( ros::NodeHandle& ph, const std::string& field );

	MatrixType GetQ() const;

	void ProcessInfo( const argus_msgs::FilterStepInfo& msg );

	bool IsReady() const;

private:

	unsigned int _windowLength;

	std::deque<MatrixType> _delXOuterProds;
	MatrixType _lastFSpostFT;
	MatrixType _currSpost;
	MatrixType _lastF;
	MatrixType _offset;
	double _lastDt;

	void InfoCallback( const argus_msgs::FilterStepInfo::ConstPtr& msg );

};

}