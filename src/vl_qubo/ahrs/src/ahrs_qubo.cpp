#include "ahrs_qubo.h"
//written by Jeremy Weed

AhrsQuboNode::AhrsQuboNode(std::shared_ptr<ros::NodeHandle> n,
	int rate, std::string name, std::string device) : RamNode(n){

	ros::Time::init();
	this->name = name;
	ros::Rate loop_rate(rate);

	ahrsPub = n->advertise<sensor_msgs::Imu>("qubo/ahrs/" + name, 1000);

	//create + open the device
	//k115200 is the baud rate of the device.  Currently chosen arbitrarily
	ahrs.reset(new AHRS(device, AHRS::k115200));
	try{
		ahrs->openDevice();
		isConnected = true;
	}catch(AHRSException& ex){
		ROS_ERROR(ex.what());
		isConnected = false;
		return;
	}

	if(!ahrs->isOpen()){
		ROS_ERROR("AHRS %s didn't open succsesfully", device.c_str());
	}
	//configs the device
	ahrs->sendAHRSDataFormat();

	ROS_DEBUG("Device Info: %s", ahrs->getInfo().c_str());
}


AhrsQuboNode::~AhrsQuboNode(){
	ahrs->closeDevice();
}

void AhrsQuboNode::update(){
	static int id = 0;
	static int attmepts = 0;

	//if we aren't connected yet, lets try a few more times
	if(!isConnected){
		try{
			ahrs->openDevice();
			isConnected = true;
		}catch(AHRSException& ex){
			ROS_ERROR("Attempt %i to connect to AHRS failed.", attmepts++);
			ROS_ERROR("DEVICE NOT FOUND! ");
			ROS_ERROR(ex.what());
			if(attmepts > MAX_CONNECTION_ATTEMPTS){
				ROS_ERROR("Failed to find device, exiting node.");
				exit(-1);
			}
			return;
		}
	}

	ROS_DEBUG("Beginning to read data");
	//sit and wait for an update
	try{
		sensor_data = ahrs->pollAHRSData();
	}catch(AHRSException& ex){
		//every so often an exception gets thrown and hangs on my vm
		//This might be solved by running ROS on actual hardware
		ROS_WARN(ex.what());
		return;
	}

	ROS_DEBUG("Data has been read");
	msg.header.stamp = ros::Time::now();
	msg.header.seq = ++id;
	msg.header.frame_id = "ahrs";

	msg.orientation.x = sensor_data.quaternion[0];
	msg.orientation.y = sensor_data.quaternion[1];
	msg.orientation.z = sensor_data.quaternion[2];
	msg.orientation.w = sensor_data.quaternion[3];
	// the -1's imply we don't know the covariance
	msg.orientation_covariance[0] = -1;

	msg.angular_velocity.x = sensor_data.gyroX;
	msg.angular_velocity.y = sensor_data.gyroY;
	msg.angular_velocity.z = sensor_data.gyroZ;
	msg.angular_velocity_covariance[0] = -1;

	msg.linear_acceleration.x = sensor_data.accelX;
	msg.linear_acceleration.y = sensor_data.accelY;
	msg.linear_acceleration.z = sensor_data.accelZ;
	msg.linear_acceleration_covariance[0] = -1;

	ahrsPub.publish(msg);
}