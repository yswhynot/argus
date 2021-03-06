#pragma once

#include "argus_utils/geometry/PoseSE3.h"
#include "nav_msgs/Odometry.h"
#include "fieldtrack/TargetState.h"

#include "geometry_msgs/Pose.h"
#include "geometry_msgs/PoseStamped.h"
#include "geometry_msgs/PoseWithCovarianceStamped.h"

#include "geometry_msgs/Twist.h"
#include "geometry_msgs/TwistStamped.h"
#include "geometry_msgs/TwistWithCovarianceStamped.h"

#include "geometry_msgs/TransformStamped.h"
#include "sensor_msgs/Imu.h"

#include <boost/variant.hpp>
#include <memory>

namespace argus
{

// Types of filter covariance models
enum CovarianceMode
{
	COV_PASS,     // Pass-through
	COV_FIXED,    // Fixed value
	COV_ADAPTIVE, // Window adaptive 
	//COV_PREDICTIVE // TODO
};
CovarianceMode StringToCovMode( const std::string& str );
std::string CovModeToString( CovarianceMode mode );

// TODO: Print functions for all structs

/*! \brief C++ counterpart of nav_msgs::Odometry message.
*/
struct TargetState
{
	std::string referenceFrame;
	std::string bodyFrame;
	ros::Time timestamp;
	PoseSE3 pose;
	VectorType derivatives;
	MatrixType covariance;
	
	TargetState();
	TargetState( const fieldtrack::TargetState& state );
	TargetState( const nav_msgs::Odometry& odom );

	fieldtrack::TargetState ToStateMsg() const;
	nav_msgs::Odometry ToOdometryMsg() const;
};

typedef boost::variant< geometry_msgs::PoseStamped,
                        geometry_msgs::PoseWithCovarianceStamped,
                        geometry_msgs::TwistStamped,
                        geometry_msgs::TwistWithCovarianceStamped,
                        geometry_msgs::TransformStamped,
                        sensor_msgs::Imu >
        ObservationMessage;

struct ObservationBase
{
	ros::Time timestamp;
	std::string referenceFrame;
};

struct PoseObservation : public ObservationBase
{
	PoseSE3 pose;
	MatrixType covariance;
};

struct PositionObservation : public ObservationBase
{
	Translation3Type position;
	MatrixType covariance;
};

struct OrientationObservation : public ObservationBase
{
	QuaternionType orientation;
	MatrixType covariance;
};

struct DerivObservation : public ObservationBase
{
	VectorType derivatives;
	MatrixType covariance;
	std::vector<unsigned int> indices;
};

typedef boost::variant< PoseObservation, 
                        PositionObservation, 
                        OrientationObservation,
                        DerivObservation > 
        Observation;

// Various observation visitors
struct ObservationTimestampVisitor
: public boost::static_visitor<ros::Time>
{
	ObservationTimestampVisitor();

	ros::Time operator()( const ObservationBase& obs ) const ;
};

struct ObservationFrameVisitor
: public boost::static_visitor<std::string>
{
	ObservationFrameVisitor();

	std::string operator()( const ObservationBase& obs ) const ;
};

ros::Time get_observation_timestamp( const Observation& obs );
std::string get_observation_frame( const Observation& obs );

}
