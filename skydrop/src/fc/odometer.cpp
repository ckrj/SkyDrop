/*
 * odometer.cpp
 *
 *  Created on: 16.02.2017
 *      Author: tilmann@bubecks.de
 */

#include "odometer.h"
#include "fc.h"

/**
 * Returns the bearing from lat1/lon1 to lat2/lon2. All parameters
 * must be given as fixed integers multiplied with GPS_MULT.
 *
 * \param lat1 the latitude of the 1st GPS point
 * \param lon1 the longitude of the 1st GPS point
 * \param lat2 the latitude of the 2nd GPS point
 * \param lon2 the longitude of the 2nd GPS point
 *
 * \return the bearing in degrees (0-359, where 0 is north, 90 is east, ...).
 */
int16_t gps_bearing(int32_t lat1, int32_t lon1,
					int32_t lat2, int32_t lon2)
{
	double dX = ((double)lon1 - lon2) / GPS_MULT;
	double dY = ((double)lat1 - lat2) / GPS_MULT;
	return ((int16_t)to_degrees(atan2(dX,dY)) + 360) % 360;
}

/**
 * Compute the distance between two GPS points in 2 dimensions
 * (without altitude). Latitude and longitude parameters must be given as fixed integers
 * multiplied with GPS_MULT.
 *
 * \param lat1 the latitude of the 1st GPS point
 * \param lon1 the longitude of the 1st GPS point
 * \param lat2 the latitude of the 2nd GPS point
 * \param lon2 the longitude of the 2nd GPS point
 *
 * \return the distance in cm.
 */
uint32_t gps_distance_2d(int32_t lat1, int32_t lon1,
			 	 	 	 int32_t lat2, int32_t lon2)
{
	double dx, dy;   // in cm
	double lat;

	// Compute the average lat of lat1 and lat2 to get the width of a
	// 1 degree cell at that position of the earth:
	lat = (lat1 + lat2) / 2 / GPS_MULT * (M_PI / 180.0);

	// 111.3 km (in cm) is the width of 1 degree
	dx = cos(lat) * 11130000 * abs(lon1 - lon2) / GPS_MULT;   // Todo: Use lcd_disp.get_cos()
	dy = 1.0      * 11130000 * abs(lat1 - lat2) / GPS_MULT;

	return (uint32_t)sqrt(dx * dx + dy * dy);
}

/**
 * Compute the distance between two GPS points in 3 dimensions
 * (including altitude). Latitude and longitude parameters must be
 * given as fixed integers multiplied with GPS_MULT.
 *
 * \param lat1 the latitude of the 1st GPS point
 * \param lon1 the longitude of the 1st GPS point
 * \param alt1 the altitude of the 1st GPS point (in m)
 * \param lat2 the latitude of the 2nd GPS point
 * \param lon2 the longitude of the 2nd GPS point
 * \param alt2 the altitude of the 2nd GPS point (in m)
 *
 * \return the distance in cm.
 */
uint32_t gps_distance_3d(int32_t lat1, int32_t lon1, double alt1,
			 	 	 	 int32_t lat2, int32_t lon2, double alt2)
{
	uint32_t dx, dy;
	float da;

	dx = gps_distance_2d(lat1, lon1, lat1, lon2);
	dy = gps_distance_2d(lat1, lon1, lat2, lon1);
	da = abs(alt1 - alt2) * 100;                 // convert from m to cm

	return (uint32_t)sqrt((double)dx * dx + (double)dy * dy + da * da);
}

/**
 * The GPS position has changed. Compute the distance to the previous
 * position and update the odometer accordingly.
 */
void odometer_step()
{
	#define NO_LAT_DATA  ((int32_t)2147483647)

	static int32_t last_lat = NO_LAT_DATA;
	static int32_t last_lon;
	static float last_alt;

	if (fc.gps_data.new_sample & FC_GPS_NEW_SAMPLE_ODO)
		fc.gps_data.new_sample &= ~FC_GPS_NEW_SAMPLE_ODO;
	else
		return;

	if (fc.flight.home_valid)
	{
		fc.flight.home_bearing = gps_bearing(config.home.lat, config.home.lon, fc.gps_data.latitude, fc.gps_data.longtitude );
		fc.flight.home_distance = gps_distance_2d(fc.gps_data.latitude, fc.gps_data.longtitude, config.home.lat, config.home.lon) / 100000.0;   // cm to km
	}

	// Do we already have a previous GPS point?
	if (last_lat != NO_LAT_DATA)
	{
		uint32_t v = gps_distance_3d(last_lat, last_lon, last_alt, fc.gps_data.latitude, fc.gps_data.longtitude, fc.gps_data.altitude);

		//calculated speed in knots
		uint16_t calc_speed = (v * FC_MPS_TO_KNOTS) / 100;

		//do not add when gps speed is < 1 km/h
		//do not add when difference between calculated speed and gps speed id > 10 km/h
		if (abs(calc_speed - fc.gps_data.groud_speed) < FC_ODO_MAX_SPEED_DIFF && fc.gps_data.groud_speed > FC_ODO_MIN_SPEED)
			fc.odometer += v;
	}

	// Save the current GPS position for the next step
	last_lat = fc.gps_data.latitude;
	last_lon = fc.gps_data.longtitude;
	last_alt = fc.gps_data.altitude;
}
