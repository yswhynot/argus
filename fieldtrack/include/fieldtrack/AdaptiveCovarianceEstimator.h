#pragma once

#include <ros/ros.h>
#include "fieldtrack/utils/FilterInfo.h"
#include <deque>

// TODO Use and generalize
// #include "broadcast/BroadcastTransmitter.h"

namespace argus
{

/*! \brief Estimates the transition covariance of a Kalman filter.
 *
 * Implementation of the AKF approach detailed in Mohamed and 
 * Schwarz 1999. Modified to use decaying weights on the correction
 * outer products to more smoothly handle outliers and incorporate a prior.
 *
 */
class AdaptiveTransitionCovarianceEstimator
{
public:

	AdaptiveTransitionCovarianceEstimator();

	// Read parameters for the estimator
	void Initialize( ros::NodeHandle& ph );

	unsigned int NumSamples() const;
	MatrixType GetQ( const ros::Time& time );

	void Update( const ros::Time& time,
	             const PredictInfo& predict, 
	             const UpdateInfo& update );

	void Reset();

private:

	typedef std::pair<ros::Time,MatrixType> InnoStamped;

	unsigned int _maxSamples;
	ros::Duration _maxAge;

	MatrixType _priorCov;
	double _priorAge;
	double _priorDt;

	bool _useDiag;
	double _decayRate;

	std::deque<InnoStamped> _innoProds; // Ordered so head is newest
	MatrixType _lastFSpostFT;
	MatrixType _currSpost;

	void CheckBuffer( const ros::Time& now );
};

class AdaptiveObservationCovarianceEstimator
{
public:

	AdaptiveObservationCovarianceEstimator();

	void Initialize( ros::NodeHandle& ph );

	unsigned int NumSamples() const;
	MatrixType GetR( const ros::Time& time );

	void Update( const ros::Time& time, const UpdateInfo& update );

	void Reset();

private:

	typedef std::pair<ros::Time,MatrixType> InnoStamped;

	unsigned int _maxSamples;
	ros::Duration _maxAge;

	MatrixType _priorCov;
	double _priorAge;

	bool _useDiag;
	double _decayRate;

	std::deque<InnoStamped> _innoProds; // Ordered so head is newest
	MatrixType _lastHPHT;

	void CheckBuffer( const ros::Time& now );
};

}