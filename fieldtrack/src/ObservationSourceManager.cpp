#include "fieldtrack/ObservationSourceManager.h"
#include "argus_utils/utils/ParamUtils.h"
#include "argus_utils/geometry/GeometryUtils.h"

#define POSE_DIM (PoseSE3::TangentDimension)
#define POSITION_DIM (PoseSE3::TangentDimension/2)
#define ORIENTATION_DIM (PoseSE3::TangentDimension/2)
#define IMU_IND_OFFSET (PoseSE3::TangentDimension/2)

namespace argus
{

ObservationSourceManager::ObservationSourceManager( ros::NodeHandle& ph )
{
	std::string mode;
	GetParamRequired( ph, "mode", mode );
	_mode = StringToCovMode( mode );

	if( _mode == COV_PASS )
	{
		// Nothing to parse
	}
	else if( _mode == COV_FIXED )
	{
		unsigned int dim;
		GetParamRequired( ph, "dim", dim );
		_fixedCov = MatrixType( dim, dim );
		GetParamRequired<double>( ph, "fixed_covariance", _fixedCov );
	}
	else if( _mode == COV_ADAPTIVE )
	{
		if( HasParam( ph, "dim" ) )
		{
			unsigned int dim;
			GetParam( ph, "dim", dim );
			_fixedCov = MatrixType( dim, dim );
			GetParam<double>( ph, "initial_covariance", _fixedCov );
		}

		_adaptiveCov.Initialize( ph );
	}
}

void ObservationSourceManager::Update( const UpdateInfo& info )
{
	if( _mode == COV_ADAPTIVE )
	{
		_adaptiveCov.Update( info );
	}
}

void ObservationSourceManager::Reset()
{
	if( _mode == COV_ADAPTIVE )
	{
		_adaptiveCov.Reset();
	}
}

Observation
ObservationSourceManager::ParsePoseMessage( const std_msgs::Header& header,
                                            const PoseSE3& pose,
                                            const MatrixType& cov,
                                            const std::vector<unsigned int>& valid )
{
	std::vector<bool> mask( POSE_DIM, false );
	for( unsigned int i = 0; i < valid.size(); i++ )
	{
		mask[ valid[i] ] = true;
	}
	bool pos = mask[0] && mask[1] && mask[2];
	bool ori = mask[3] && mask[4] && mask[5];
	
	if( pos && ori )
	{
		PoseObservation obs;
		obs.timestamp = header.stamp;
		obs.referenceFrame = header.frame_id;
		obs.pose = pose;
		obs.covariance = cov;
		return obs;
	}
	else if( pos )
	{
		PositionObservation obs;
		obs.timestamp = header.stamp;
		obs.referenceFrame = header.frame_id;
		obs.position = pose.GetTranslation();
		obs.covariance = (cov.rows() == POSITION_DIM) ? cov : MaskMatrix( cov, valid );
		return obs;
	}
	else if( ori )
	{
		OrientationObservation obs;
		obs.timestamp = header.stamp;
		obs.referenceFrame = header.frame_id;
		obs.orientation = pose.GetQuaternion();
		obs.covariance = (cov.rows() == ORIENTATION_DIM) ? cov : MaskMatrix( cov, valid );
		return obs;
	}
	throw std::invalid_argument( "Pose message has invalid configuration." );
}

Observation
ObservationSourceManager::ParseDerivatives( const std_msgs::Header& header,
                                            const VectorType& derivs,
                                            const MatrixType& cov,
                                            const std::vector<unsigned int>& valid )
{
	DerivObservation obs;
	obs.timestamp = header.stamp;
	obs.referenceFrame = header.frame_id;
	obs.indices = valid;
	obs.derivatives = (derivs.size() != valid.size()) ? MaskVector( derivs, valid )
	                                                  : derivs;
	obs.covariance = (cov.rows() != valid.size()) ? MaskMatrix( cov, valid )
	                                              : cov;
	return obs;
}

Observation
ObservationSourceManager::ParseImu( const std_msgs::Header& header,
                                    const VectorType& derivs,
                                    const MatrixType& cov,
                                    const std::vector<unsigned int>& valid )
{
	DerivObservation obs;
	obs.timestamp = header.stamp;
	obs.referenceFrame = header.frame_id;
	obs.indices.clear();
	for( unsigned int i = 0; i < valid.size(); i++ )
	{
		// IMU obs start with angular velocities, so indices need to be offset
		obs.indices.push_back( IMU_IND_OFFSET + valid[i] );
	}
	obs.derivatives = (derivs.size() != valid.size()) ? MaskVector( derivs, valid )
	                                                  : derivs;
	obs.covariance = (cov.rows() != valid.size()) ? MaskMatrix( cov, valid )
	                                              : cov;
	return obs;
}

Observation ObservationSourceManager::operator()( const geometry_msgs::PoseStamped& msg )
{
	PoseSE3 pose = MsgToPose( msg.pose );
	MatrixType cov( POSE_DIM, POSE_DIM );
	
	std::vector<unsigned int> valid;
	if( _mode == COV_PASS )
	{
		throw std::invalid_argument( "PoseStamped cannot have COV_PASS mode." );
	}
	else if( _mode == COV_FIXED )
	{
		cov = _fixedCov;
		CheckMatrixSize( cov, POSE_DIM );
		valid = ParseCovarianceMask( cov );
	}
	else if( _mode == COV_ADAPTIVE )
	{
		if( _fixedCov.size() == 0 )
		{
			throw std::runtime_error( "PoseStamped does not have init_covariance in COV_ADAPTIVE mode." );
		}
		CheckMatrixSize( _fixedCov, POSE_DIM );
		valid = ParseCovarianceMask( _fixedCov );
		cov = _adaptiveCov.IsReady() ? _adaptiveCov.GetR() : _fixedCov;
	}
	else
	{
		throw std::invalid_argument( "Unknown covariance mode." );
	}
	return ParsePoseMessage( msg.header, pose, cov, valid );
}

Observation ObservationSourceManager::operator()( const geometry_msgs::PoseWithCovarianceStamped& msg )
{
	PoseSE3 pose = MsgToPose( msg.pose.pose );
	MatrixType cov( POSE_DIM, POSE_DIM );
	ParseMatrix( msg.pose.covariance, cov );

	std::vector<unsigned int> valid;
	if( _mode == COV_PASS )
	{
		valid = ParseCovarianceMask( cov );
	}
	else if( _mode == COV_FIXED )
	{
		cov = _fixedCov;
		CheckMatrixSize( cov, POSE_DIM );
		valid = ParseCovarianceMask( cov );
	}
	else if( _mode == COV_ADAPTIVE )
	{
		MatrixType init = _fixedCov.size() == 0 ? cov : _fixedCov;
		CheckMatrixSize( init, POSE_DIM );
		valid = ParseCovarianceMask( init );
		cov = _adaptiveCov.IsReady() ? _adaptiveCov.GetR() : init;
	}
	else
	{
		throw std::invalid_argument( "Unknown covariance mode." );
	}
	return ParsePoseMessage( msg.header, pose, cov, valid );
}

Observation ObservationSourceManager::operator()( const geometry_msgs::TwistStamped& msg )
{
	VectorType derivs = MsgToTangent( msg.twist );
	MatrixType cov( POSE_DIM, POSE_DIM );
	std::vector<unsigned int> valid;
	if( _mode == COV_PASS )
	{
		throw std::runtime_error( "TwistStamped cannot have COV_PASS mode!" );
	}
	else if( _mode == COV_FIXED )
	{
		cov = _fixedCov;
		CheckMatrixSize( cov, POSE_DIM );
		valid = ParseCovarianceMask( cov );
	}
	else if( _mode == COV_ADAPTIVE )
	{
		if( _fixedCov.size() == 0 )
		{
			throw std::runtime_error( "TwistStamped does not have init_covariance in COV_ADAPTIVE mode." );
		}
		CheckMatrixSize( _fixedCov, POSE_DIM );
		valid = ParseCovarianceMask( _fixedCov );
		cov = _adaptiveCov.IsReady() ? _adaptiveCov.GetR() : _fixedCov;
	}
	else
	{
		throw std::invalid_argument( "Unknown covariance mode." );
	}
	return ParseDerivatives( msg.header, derivs, cov, valid );
}

Observation ObservationSourceManager::operator()( const geometry_msgs::TwistWithCovarianceStamped& msg )
{
	VectorType derivs = MsgToTangent( msg.twist.twist );
	
	MatrixType cov( POSE_DIM, POSE_DIM );
	ParseMatrix( msg.twist.covariance, cov );
	
	std::vector<unsigned int> valid;
	if( _mode == COV_PASS )
	{
		valid = ParseCovarianceMask( cov );
	}
	else if( _mode == COV_FIXED )
	{
		cov = _fixedCov;
		CheckMatrixSize( cov, POSE_DIM );
		valid = ParseCovarianceMask( cov );
	}
	else if( _mode == COV_ADAPTIVE )
	{
		MatrixType init = _fixedCov.size() == 0 ? cov : _fixedCov;
		CheckMatrixSize( init, POSE_DIM );
		valid = ParseCovarianceMask( init );
		cov = _adaptiveCov.IsReady() ? _adaptiveCov.GetR() : init;
	}
	else
	{
		throw std::invalid_argument( "Unknown covariance mode." );
	}
	return ParseDerivatives( msg.header, derivs, cov, valid );
}

Observation ObservationSourceManager::operator()( const sensor_msgs::Imu& msg )
{
	// TODO Support orientation updates from IMU
	// Begin by parsing all data from msg
	VectorType derivs = VectorType::Zero(POSE_DIM);
	derivs.head<3>() = MsgToVector3( msg.angular_velocity );
	derivs.tail<3>() = MsgToVector3( msg.linear_acceleration );

	// We assume no correlation between gyro/xl measurements in the message
	MatrixType cov = MatrixType::Zero(POSE_DIM,POSE_DIM);
	MatrixType temp(3,3);
	ParseMatrix( msg.angular_velocity_covariance, temp );
	cov.topLeftCorner(3,3) = temp;
	ParseMatrix( msg.linear_acceleration_covariance, temp );
	cov.bottomRightCorner(3,3) = temp;

	std::vector<unsigned int> valid;
	if( _mode == COV_PASS )
	{
		valid = ParseCovarianceMask( cov );
	}
	else if( _mode == COV_FIXED )
	{
		cov = _fixedCov;
		CheckMatrixSize( cov, POSE_DIM );
		valid = ParseCovarianceMask( cov );
	}
	else if( _mode == COV_ADAPTIVE )
	{
		MatrixType init = (_fixedCov.size() == 0) ? cov : _fixedCov;
		CheckMatrixSize( init, POSE_DIM );
		valid = ParseCovarianceMask( init );
		cov = _adaptiveCov.IsReady() ? _adaptiveCov.GetR() : init;
	}
	else
	{
		throw std::invalid_argument( "Unknown covariance mode." );
	}
	return ParseImu( msg.header, derivs, cov, valid );
}

std::vector<unsigned int>
ObservationSourceManager::ParseCovarianceMask( const MatrixType& cov )
{
	std::vector<unsigned int> inds;
	for( unsigned int i = 0; i < cov.rows(); i++ )
	{
		if( cov(i,i) >= 0 ) { inds.push_back( i ); }
	}
	return inds;
}

VectorType
ObservationSourceManager::MaskVector( const VectorType& vec, 
                                      const std::vector<unsigned int>& inds )
{
	VectorType out( inds.size() );
	GetSubmatrix( vec, out, inds );
	return out;
}

MatrixType
ObservationSourceManager::MaskMatrix( const MatrixType& mat,
                                      const std::vector<unsigned int>& inds )
{
	MatrixType out( inds.size(), inds.size() );
	GetSubmatrix( mat, out, inds, inds );
	return out;
}

void ObservationSourceManager::CheckMatrixSize( const MatrixType& mat, 
                                                unsigned int s )
{
	if( mat.rows() != s || mat.cols() != s )
	{
		std::stringstream ss;
		ss << "Matrix has size: (" << mat.rows() << ", " << mat.cols() 
		   << ") but expected " << s;
		throw std::invalid_argument( ss.str() );
	}
}

}