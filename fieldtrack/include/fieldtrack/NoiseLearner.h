#pragma once

#include <ros/ros.h>

#include "fieldtrack/CovarianceModels.h"
#include "fieldtrack/LikelihoodChain.h"

#include "argus_utils/synchronization/SynchronizationTypes.h"
#include <deque>

namespace argus
{
class NoiseLearner
{
public:

	NoiseLearner();

	void Initialize( ros::NodeHandle& ph );
	void BufferInfo( const FilterInfo& info );
	void ClearBuffer();

	void RegisterTransModel( const CovarianceModel::Ptr& model );
	void RegisterObsModel( const std::string& name,
	                       const CovarianceModel::Ptr& model );

	void LearnSpin();

private:

	unsigned int _minNumModules;
	unsigned int _maxBufferSize;

	unsigned int _numIterations;
	double _stepDecayRate;

	unsigned int _stepsPerBatch;
	double _stepSize;
	double _maxL1Norm;

	Mutex _bufferMutex; // Controls access to buffer
	Mutex _chainMutex; // Controls access to chain
	std::deque<FilterInfo> _infoBuffer;

	LikelihoodChain _chain;

	std::vector<CovarianceModel::Ptr> _obsModels;
	CovarianceModel::Ptr _transCov;

	double GetStepSize() const;
	unsigned int GetParamDim() const;
	VectorType GetDerivatives() const;
	void StepParameters( const VectorType& s );
};
}