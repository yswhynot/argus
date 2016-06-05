#include "fieldtrack/SimpleStateEstimator.h"
#include "argus_utils/geometry/GeometryUtils.h"
#include "argus_utils/utils/MatrixUtils.h"
#include "argus_utils/utils/ParamUtils.h"
#include "argus_utils/filters/FilterUtils.h"
#include "argus_utils/utils/YamlUtils.h"

using namespace argus_msgs;
using namespace geometry_msgs;
using namespace nav_msgs;


namespace argus
{

SimpleStateEstimator::SimpleStateEstimator( const ros::NodeHandle& nh, 
                                            const ros::NodeHandle& ph )
: nodeHandle( nh ), privHandle( ph ), 
filter( PoseSE3(), FilterType::DerivsType::Zero(), FilterType::FullCovType::Zero() )
{
	GetParam( privHandle, "reference_frame", referenceFrame );
	GetParam( privHandle, "body_frame", bodyFrame );
	
	// Parse all update sources
	XmlRpc::XmlRpcValue updateSources;
	GetParam( privHandle, "update_sources", updateSources );
	YAML::Node updatesYaml = XmlToYaml( updateSources );
	YAML::Node::const_iterator iter;
	for( iter = updatesYaml.begin(); iter != updatesYaml.end(); iter++ )
	{
		const std::string& sourceName = iter->first.as<std::string>();
		const std::string& topic = iter->second.as<std::string>();
		ROS_INFO_STREAM( "Subscribing to updates from " << sourceName
		                 << " at " << topic );
		updateSubs[sourceName] = nodeHandle.subscribe( topic, 10, &SimpleStateEstimator::UpdateCallback, 
		                                         this );
	}

	// Parse cov rate
	if( !GetMatrixParam<double>( privHandle, "covariance_rate", Qrate ) )
	{
		if( !GetDiagonalParam<double>( privHandle, "covariance_rate", Qrate ) )
		{
			ROS_WARN_STREAM( "No velocity covariance rate given. Using identity." );
			Qrate = FilterType::FullCovType::Identity();
		}
	}

	// Initialize
	FilterType::FullCovType initCov;
	if( !GetMatrixParam<double>( privHandle, "initial_covariance", initCov ) )
	{
		if( !GetDiagonalParam<double>( privHandle, "initial_covariance", initCov ) )
		{
			ROS_WARN_STREAM( "No initial covariance rate given. Using 10*identity." );
			initCov = FilterType::FullCovType::Identity();
		}
	}

	// TODO Initial state?
	filter.Pose() = PoseSE3();
	filter.Derivs() = FilterType::DerivsType::Zero();
	filter.FullCov() = initCov;
	filterTime = ros::Time::now();

	privHandle.param<bool>( "two_dimensional", twoDimensional, false );
	ROS_INFO_STREAM( "Two dimensional mode: " << twoDimensional );

	odomPub = nodeHandle.advertise<Odometry>( "odometry", 10 );
	stepPub = nodeHandle.advertise<argus_msgs::FilterStepInfo>( "filter_info", 10 );
	
	double timerRate;
	privHandle.param<double>( "timer_rate", timerRate, 10.0 );
	ROS_INFO_STREAM( "Publishing at " << timerRate << " Hz" );
	updateTimer = std::make_shared<ros::Timer>(
	    nodeHandle.createTimer( ros::Duration( 1.0 / timerRate ),
	                            &SimpleStateEstimator::TimerCallback,
	                            this ) );
}

void SimpleStateEstimator::UpdateCallback( const FilterUpdate::ConstPtr& msg )
{
	VectorType z;
	ParseMatrix( msg->observation, z );
	MatrixType C = MsgToMatrix( msg->observation_matrix );
	MatrixType R = MsgToMatrix( msg->observation_cov );

	if( C.cols() != FilterType::CovarianceDim )
	{
		ROS_WARN_STREAM( "Update C has wrong dimension." );
	}

	// TODO Case where there are no derivs in the filter?
	bool hasPosition = !( C.block( 0, 0, C.rows(), 3 ).array() == 0 ).all();
	bool hasOrientation = !( C.block( 0, 3, C.rows(), 3 ).array() == 0 ).all();
	bool hasDerivs = !( C.rightCols( C.rows() - 6 ).array() == 0 ).all();

	// First predict up to the update time
	ros::Time updateTime = msg->header.stamp;
	if( updateTime > filterTime )
	{
		double dt = (updateTime - filterTime).toSec();
		PredictInfo info = filter.Predict( Qrate*dt, dt );
		argus_msgs::FilterStepInfo pmsg = PredictToMsg( info );
		pmsg.header.stamp = msg->header.stamp;
		stepPub.publish( pmsg );
		filterTime = updateTime;
	}
	else if( updateTime < filterTime )
	{
		// TODO Implement filter reversing
		ROS_WARN_STREAM( "Update received from before filter time." );
	}

	FilterStepInfo info;
	if( hasPosition && !hasOrientation && !hasDerivs )
	{
		ROS_WARN_STREAM( "Position only updates are not supported yet." );
		return;
	}
	else if( !hasPosition && hasOrientation && !hasDerivs )
	{
		ROS_WARN_STREAM( "Orientation only updates are not supported yet." );
		return;
	}
	else if( hasPosition && hasOrientation && !hasDerivs )
	{
		info = PoseUpdate( PoseSE3( z ), R );
	}
	else if( !hasPosition && !hasOrientation && hasDerivs )
	{
		info = DerivsUpdate( z, C, R );
	}
	else if( hasPosition && hasOrientation && hasDerivs )
	{
		// PoseSE3 pose;
		// pose.FromVector( z.head(7) );
		// VectorType derivs = z.tail( z.size() - 7 );
		// info = JointUpdate( pose, derivs, C, R );
		ROS_WARN_STREAM( "Joint update not supported yet." );
		return;
	}
	else
	{
		ROS_WARN_STREAM( "Unsupported update combination." );
		return;
	}
	
	info.header = msg->header;
	stepPub.publish( info );

}

argus_msgs::FilterStepInfo
SimpleStateEstimator::PoseUpdate( const PoseSE3& pose, 
                                  const MatrixType& R )
{
	UpdateInfo info = filter.UpdatePose( pose, R );
	return UpdateToMsg( info );
	
}

argus_msgs::FilterStepInfo 
SimpleStateEstimator::DerivsUpdate( const VectorType& derivs, 
                                    const MatrixType& C,
                                    const MatrixType& R )
{
	FilterType::DerivObsMatrix Cderivs = C.rightCols( C.cols() - 6 );
	UpdateInfo info = filter.UpdateDerivs( derivs, Cderivs, R );
	return UpdateToMsg( info );
}

// argus_msgs::FilterStepInfo PositionUpdate( const Translation3Type& pos, 
//                                            const MatrixType& R )
// {}

// argus_msgs::FilterStepInfo OrientationUpdate( const QuaternionType& ori, 
//                                               const MatrixType& R )
// {}

// void SimpleStateEstimator::JointUpdate( const PoseSE3& pose,
//                                         const VectorType& derivs, 
//                                         const MatrixType& C,
//                                         const MatrixType& R )
// {}

void SimpleStateEstimator::TimerCallback( const ros::TimerEvent& event )
{
	ros::Time now = event.current_real;
	if( now > filterTime )
	{
		double dt = (now - filterTime).toSec();
		PredictInfo info = filter.Predict( Qrate*dt, dt );
		argus_msgs::FilterStepInfo pmsg = PredictToMsg( info );
		pmsg.header.stamp = event.current_real;
		stepPub.publish( pmsg );
		filterTime = now;
	}
	if( twoDimensional ) { EnforceTwoDimensionality(); }

	Odometry msg;
	msg.header.frame_id = referenceFrame;
	msg.header.stamp = event.current_real;
	msg.child_frame_id = bodyFrame;
	
	msg.pose.pose = PoseToMsg( filter.Pose() );
	SerializeMatrix( filter.PoseCov(), msg.pose.covariance );
	
	msg.twist.twist = TangentToMsg( filter.Derivs().head<6>() );
	SerializeMatrix( filter.DerivsCov().topLeftCorner<6,6>(), msg.twist.covariance );
	
	odomPub.publish( msg );
}

void SimpleStateEstimator::EnforceTwoDimensionality()
{
  FixedVectorType<7> poseVector = filter.Pose().ToVector();
  PoseSE3 mean( poseVector(0), poseVector(1), 0, poseVector(3), 0, 0, poseVector(6) );
  filter.Pose() = mean;

  PoseSE3::TangentVector velocity = filter.Derivs().head<6>();
  velocity(2) = 0;
  velocity(3) = 0;
  velocity(4) = 0;
  filter.Derivs().head<6>() = velocity;
}

}
