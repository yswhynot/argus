#include "manycal/CameraArrayCalibrator.h"
#include "argus_utils/utils/ParamUtils.h"

#include <boost/foreach.hpp>

using namespace fiducials;

namespace argus
{

CameraArrayCalibrator::CameraArrayCalibrator( ros::NodeHandle& nh,
                                              ros::NodeHandle& ph )
: _fiducialManager( _lookupInterface ),
  _extrinsicsManager( nh, ph )
{
	GetParamRequired( ph, "use_synchronization", _useSynchronization );
	if( _useSynchronization )
	{
		double dt;
		unsigned int buffLen;
		GetParamRequired( ph, "sync_max_dt", dt );
		GetParamRequired( ph, "sync_buff_len", buffLen );
		_sync.SetMaxDt( dt );
		_sync.SetBufferLength( buffLen );
	}

	GetParamRequired( ph, "reference_frame", _referenceFrame );
	GetParam( ph, "prior_covariance", _priorCovariance, PoseSE3::CovarianceMatrix::Identity() );

	std::string lookupNamespace;
	GetParam<std::string>( ph, "lookup_namespace", lookupNamespace, "/lookup" );
	_lookupInterface.SetLookupNamespace( lookupNamespace );
	
	// Create optimizer object
	ros::NodeHandle oh( ph.resolveName( "optimizer" ) );
	_optimizer = GraphOptimizer( oh );
	
	// _writeServer = ph.advertiseService( "write_results", 
	//                                     &CameraArrayCalibrator::WriteResults,
	//                                     this );
	unsigned int buffSize;
	GetParam( ph, "buffer_size", buffSize, (unsigned int) 10 );
	_detSub = nh.subscribe( "detections", 
	                        buffSize, 
	                        &CameraArrayCalibrator::DetectionCallback, 
	                        this );
}
	
bool CameraArrayCalibrator::WriteResults( manycal::WriteCalibration::Request& req,
                                          manycal::WriteCalibration::Response& res )
{
	YAML::Node yaml;
	BOOST_FOREACH( const CameraRegistry::value_type& item, _cameraRegistry )
	{
		const std::string& name = item.first;
		const CameraRegistration& registration = item.second;
		PoseSE3 extrinsics = registration.extrinsics->value().pose;
		ROS_INFO_STREAM( "Camera " << name << " pose " << extrinsics );
		
		// ExtrinsicsInfo info;
		// info.extrinsics = extrinsics;
		// info._referenceFrame = _referenceFrame;
		// extrinsicsManager.WriteMemberInfo( name, info, true );

		// YAML::Node node;
		// PopulateExtrinsicsCalibration( extrinsicsManager.GetInfo( name ), node );
		// yaml[name] = node;
	}

	// std::ofstream resultsFile( req.calibrationPath );
	// if( !resultsFile.is_open() )
	// {
	// 	ROS_ERROR_STREAM( "Could not open results file at: " << req.calibrationPath );
	// 	return false;
	// }
	// ROS_INFO_STREAM( "Writing results to " << req.calibrationPath );
	// resultsFile << yaml;
	return true;
}

std::vector<FiducialCalibration> CameraArrayCalibrator::GetFiducials() const
{
	std::vector<FiducialCalibration> fids;
	FiducialCalibration cal;

	typedef FiducialRegistry::value_type Item;
	BOOST_FOREACH( const Item& item, _fiducialRegistry )
	{
		const std::string& name = item.first;
		const FiducialRegistration& reg = item.second;
		
		cal.intrinsics = IsamToFiducial( reg.intrinsics->value() );

		typedef std::map<ros::Time, isam::PoseSE3_Node::Ptr>::value_type Subitem;
		BOOST_FOREACH( const Subitem& sub, reg.poses )
		{
			const ros::Time& t = sub.first;
			const isam::PoseSE3_Node::Ptr& pose = sub.second;
			std::stringstream ss;
			ss << name << "_" << t;
			cal.name = ss.str();
			cal.extrinsics = pose->value().pose;
			fids.push_back( cal );
		}
	}
	return fids;
}

std::vector<CameraCalibration> CameraArrayCalibrator::GetCameras() const
{
	std::vector<CameraCalibration> cams;
	CameraCalibration cal;

	typedef CameraRegistry::value_type Item;
	BOOST_FOREACH( const Item& item, _cameraRegistry )
	{
		const std::string& name = item.first;
		const CameraRegistration& reg = item.second;;
		cal.name = name;
		cal.extrinsics = reg.extrinsics->value().pose;
		// cal.intrinsics = IsamToMonocular( reg.intrinsics->value() );
		cams.push_back( cal );
	}
	return cams;
}

void CameraArrayCalibrator::ProcessCache()
{
	std::vector <ImageFiducialDetections> recache;
	BOOST_FOREACH( const ImageFiducialDetections& dets, _cachedObservations )
	{
		if( !ProcessDetection( dets ) )
		{
			recache.push_back( dets );
		}
	}
	_cachedObservations = recache;

	// Don't optimize when we don't have any observations!
	if( _observations.size() == 0 ) { return; }
	_optimizer.GetOptimizer().update();
}

void CameraArrayCalibrator::DetectionCallback( const argus_msgs::ImageFiducialDetections::ConstPtr& msg )
{

	if( _useSynchronization )
	{
		ImageFiducialDetections det( *msg );
		try
		{
			_sync.BufferData( det.sourceName, msg->header.stamp.toSec(), det );
		}
		catch( std::invalid_argument& e )
		{
			_sync.RegisterSource( det.sourceName );
			_sync.BufferData( det.sourceName, msg->header.stamp.toSec(), det );		
		}

		while( _sync.HasOutput() )
		{
			DetectionSynchronizer::DataMap output = _sync.GetOutput();

			ros::Time latestTime( 0 );
			typedef DetectionSynchronizer::DataMap::value_type Item;
			BOOST_FOREACH( Item& item, output )
			{
				ros::Time time = item.second.second.timestamp;
				if( time > latestTime ) { latestTime = time; }
			}
			BOOST_FOREACH( Item& item, output )
			{
				ImageFiducialDetections& det = item.second.second;
				det.timestamp = latestTime;
				_cachedObservations.emplace_back( det );
			}
		}
	}
	else 
	{
		 _cachedObservations.emplace_back( *msg );
	}
	ProcessCache();
}

bool CameraArrayCalibrator::ProcessDetection( const ImageFiducialDetections& dets )
{
	// Attempt to register all new fiducials
	BOOST_FOREACH( const FiducialDetection& det, dets.detections )
	{
		if( _fiducialRegistry.count( det.name ) == 0 && 
		    _fiducialManager.CheckMemberInfo( det.name, false ) )
		{
			RegisterFiducial( det.name );
		}
	}
	
	// Attempt to initialize the camera if new
	if( _cameraRegistry.count( dets.sourceName ) == 0 &&
	    !InitializeCamera( dets ) )
	{
		ROS_WARN_STREAM( "Could not initialize camera: " << dets.sourceName <<
		                 " from detections. Skipping..." );
		return false;
	}
	const CameraRegistration& camReg = _cameraRegistry.at( dets.sourceName );
	
	// Process detections
	BOOST_FOREACH( const FiducialDetection& det, dets.detections )
	{
		if( _fiducialRegistry.count( det.name ) == 0 ) { continue; }

		FiducialRegistration& fidReg = _fiducialRegistry[ det.name ];
		// Initialize fiducial if first detection at this time
		if( fidReg.poses.count( dets.timestamp ) == 0 )
		{
			PoseSE3 cameraPose = camReg.extrinsics->value().pose;
			PoseSE3 relPose = EstimateArrayPose( det,
			                                     IsamToFiducial( fidReg.intrinsics->value() ) );
			PoseSE3 fiducialPose = cameraPose * relPose;
			ROS_INFO_STREAM( "Initializing fiducial at pose: " << fiducialPose );
			isam::PoseSE3_Node::Ptr fidNode = std::make_shared<isam::PoseSE3_Node>();
			fidNode->init( isam::PoseSE3( fiducialPose ) );
			fidReg.poses[ dets.timestamp ] = fidNode;
			_optimizer.GetOptimizer().add_node( fidNode.get() );
		}
		
		isam::PoseSE3_Node::Ptr fidNode = fidReg.poses[ dets.timestamp ];
		
		double imageCoordinateErr = std::pow( 0.03, 2 );
		isam::Noise cov = isam::Covariance( imageCoordinateErr * isam::eye( 2*det.points.size() ) );
		
		isam::FiducialFactor::Properties props;
		props.optCamReference = true; // Reference is actually extrinsics here
		props.optCamIntrinsics = false;
		props.optCamExtrinsics = false;
		props.optFidReference = true;
		props.optFidIntrinsics = false;
		props.optFidExtrinsics = false;
		
		isam::FiducialFactor::Ptr factor = std::make_shared<isam::FiducialFactor>
			( camReg.extrinsics.get(), 
			  camReg.intrinsics.get(), 
			  nullptr,
			  fidNode.get(), 
			  fidReg.intrinsics.get(), 
			  nullptr,
			  DetectionToIsam( det ), 
			  cov, 
			  props );
		ROS_INFO_STREAM( "Adding detection" );
		_optimizer.GetOptimizer().add_factor( factor.get() );
		_observations.push_back( factor );
	}
	
	return true;
}

// Attempt to initialize a camera from the detection
bool CameraArrayCalibrator::InitializeCamera( const ImageFiducialDetections& dets )
{
	if( _cameraRegistry.count( dets.sourceName ) > 0 ) { return true; }
	
	// First see if this camera has an extrinsics prior
	try
	{
		PoseSE3 extrinsics = _extrinsicsManager.GetExtrinsics( dets.sourceName,
		                                                       _referenceFrame );
		RegisterCamera( dets.sourceName, extrinsics, true );
		return true;
	}
	catch( ExtrinsicsException& e ) {}

	// If not, see if there if any observed fiducial have poses
	BOOST_FOREACH( const FiducialDetection& det, dets.detections )
	{
		// If fiducial not registered
		if( _fiducialRegistry.count( det.name ) == 0 ) { continue; }
		FiducialRegistration& fidReg = _fiducialRegistry[ det.name ];

		// If fiducial pose at this time is not initialized yet
		if( fidReg.poses.count( dets.timestamp ) == 0 ) { continue; } 
		
		PoseSE3 fiducialPose = fidReg.poses[ dets.timestamp ]->value().pose;
		PoseSE3 relPose = EstimateArrayPose( det,
		                                     IsamToFiducial( fidReg.intrinsics->value() ) );
		PoseSE3 cameraPose = fiducialPose * relPose.Inverse();
		RegisterCamera( dets.sourceName, cameraPose, false );
		return true;
	}
	return false;
}

void CameraArrayCalibrator::RegisterCamera( const std::string& name, 
                                            const PoseSE3& pose,
                                            bool addPrior )
{
	if( _cameraRegistry.count( name ) > 0 ) { return; }
	
	ROS_INFO_STREAM( "Registering camera " << name << " at pose " << pose );
	
	CameraRegistration registration;
	registration.extrinsics = std::make_shared<isam::PoseSE3_Node>();
	registration.extrinsics->init( isam::PoseSE3( pose ) );
	_optimizer.GetOptimizer().add_node( registration.extrinsics.get() );
	
	registration.intrinsics = std::make_shared<isam::MonocularIntrinsics_Node>();
	registration.intrinsics->init( isam::MonocularIntrinsics( 1, 1, Eigen::Vector2d( 0, 0 ) ) );
	// TODO: Not optimizing intrinsics yet?
	
	if( addPrior )
	{
		ROS_INFO_STREAM( "Adding a prior for " << name );
		isam::Noise priorCov = isam::Covariance( _priorCovariance );
		registration.extrinsicsPrior = 
			std::make_shared<isam::PoseSE3_Prior>( registration.extrinsics.get(),
			                                       isam::PoseSE3( pose ),
			                                       priorCov );
		_optimizer.GetOptimizer().add_factor( registration.extrinsicsPrior.get() );
	}
	
	_cameraRegistry[ name ] = registration;
}

void CameraArrayCalibrator::RegisterFiducial( const std::string& name )
{
	if( _fiducialRegistry.count( name ) > 0 ) { return; }
	
	ROS_INFO_STREAM( "Registering fiducial " << name );

	FiducialRegistration registration;
	isam::FiducialIntrinsics intrinsics = FiducialToIsam( _fiducialManager.GetInfo( name ) );
	registration.intrinsics = std::make_shared<isam::FiducialIntrinsics_Node>
	    ( intrinsics.name(), intrinsics.dim() );
	registration.intrinsics->init( intrinsics );
	// TODO Not optimizing intrinsics yet?
	
	_fiducialRegistry[ name ] = registration;
}

}
