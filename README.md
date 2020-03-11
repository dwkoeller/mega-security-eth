# mega_security

Arduino security panel interface using Robotdyn Mega ETH board.

Currently monitors up to 28 zones ever .5 sec and reports to Home Assistant binary sensors
Web based interface for setting up zones.  Zone info stored in eeprom
Admin password stored in eeprom with Web Panel to change password

TODO

Add serial link to remote panel.

NOTE:

Pubsubclient.h MQTT_MAX_PACKET_SIZE must be increased from 128 to 256
