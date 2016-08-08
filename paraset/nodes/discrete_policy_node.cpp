#include "paraset/DiscretePolicyManager.h"
#include <ros/ros.h>

using namespace argus;

int main( int argc, char** argv )
{
	ros::init( argc, argv, "discrete_policy_node" );

	ros::NodeHandle ph( "~" );
	DiscretePolicyManager policy;
	policy.Initialize( ph );
	ros::spin();
	return 0;
}