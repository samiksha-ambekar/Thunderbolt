/*
 *     SocialLedge.com - Copyright (C) 2013
 *
 *     This file is part of free software framework for embedded processors.
 *     You can use it and/or distribute it as long as this copyright header
 *     remains unmodified.  The code is free for personal use and requires
 *     permission to use in a commercial product.
 *
 *      THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 *      OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 *      MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 *      I SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR
 *      CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 *     You can reach the author of this software at :
 *          p r e e t . w i k i @ g m a i l . c o m
 */

/**
 * @file
 * This contains the period callback functions for the periodic scheduler
 *
 * @warning
 * These callbacks should be used for hard real-time system, and the priority of these
 * tasks are above everything else in the system (above the PRIORITY_CRITICAL).
 * The period functions SHOULD NEVER block and SHOULD NEVER run over their time slot.
 * For example, the 1000Hz take slot runs periodically every 1ms, and whatever you
 * do must be completed within 1ms.  Running over the time slot will reset the system.
 */

#include <stdint.h>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "io.hpp"
#include "periodic_callback.h"
#include "_can_dbc\generated_can.h"
#include "can.h"

#include <string>
#include "tasks.hpp"
#include "examples/examples.hpp"
#include "gpio.hpp"
#include "uart2.hpp"
#include "uart3.hpp"
#include "utilities.h"
using namespace std;

int i = 0;
string Latlng = "";
void *p;
long double latitude[100];
long double longitude[100];
bool flag1 = false;
bool flag2 = false;
bool flag3 = false;
bool isACK = false;
bool isInitialLocation = false;
bool isClickEnabled = false;
bool isStopEnabled = false;
COM_BRIDGE_CHECK_POINT_t m ={ 0 };
COM_BRIDGE_HEARTBEAT_t hb = { 0 };
COM_BRIDGE_STOPALL_t stopSig = { 0 };
GPS_CURRENT_LOCATION_t gpsData = { 0 };
GPS_ACKNOWLEDGEMENT_t gpsACK = { 0 };
COM_BRIDGE_CLICKED_START_t startSig = { 0 };
int count1 = 0;
int setCount = 0;
const uint32_t                             GPS_CURRENT_LOCATION__MIA_MS = 3000;
const GPS_CURRENT_LOCATION_t               GPS_CURRENT_LOCATION__MIA_MSG = { 0 };
const uint32_t                             GPS_ACKNOWLEDGEMENT__MIA_MS = 1500;
const GPS_ACKNOWLEDGEMENT_t                GPS_ACKNOWLEDGEMENT__MIA_MSG ={ 100 };
Uart3 *u3 = &(Uart3::getInstance());


can_msg_t can_msg = { 0 };

void setupBT();
void BT(void *p);




/// This is the stack size used for each of the period tasks (1Hz, 10Hz, 100Hz, and 1000Hz)
const uint32_t PERIOD_TASKS_STACK_SIZE_BYTES = (512 * 4);

/**
 * This is the stack size of the dispatcher task that triggers the period tasks to run.
 * Minimum 1500 bytes are needed in order to write a debug file if the period tasks overrun.
 * This stack size is also used while calling the period_init() and period_reg_tlm(), and if you use
 * printf inside these functions, you need about 1500 bytes minimum
 */
const uint32_t PERIOD_DISPATCHER_TASK_STACK_SIZE_BYTES = (512 * 3);

/// Called once before the RTOS is started, this is a good place to initialize things once
bool period_init(void)
{
    // TODO : Jon : Put these into single static functions
    //              can_init();
    //              bt_init();
    //              etc
    CAN_init(can1,100,10,10,NULL,NULL);
    CAN_reset_bus(can1);
    CAN_bypass_filter_accept_all_msgs();
    u3->init(38400, 1000, 1000);
    u3->flush();
    setupBT();
    u3->putline("Nikhil#");
    //u3->put

    return true; // Must return true upon success
}

/// Register any telemetry variables
bool period_reg_tlm(void)
{
    // Make sure "SYS_CFG_ENABLE_TLM" is enabled at sys_config.h to use Telemetry
    return true; // Must return true upon success
}


/**
 * Below are your periodic functions.
 * The argument 'count' is the number of times each periodic task is called.
 */

void period_1Hz(uint32_t count)
{
    // TODO: Jon : Single functions and call them here
    //             handle_can_reset();
    //             handle_can_tx_heartbeat();

	if(CAN_is_bus_off(can1))
	{
		CAN_reset_bus(can1);
		LE.on(1);
	}
	else
	{
		LE.off(1);
	}
	hb.COM_BRIDGE_HEARTBEAT_UNSIGNED = COM_BRIDGE_HEARTBEAT_HDR.mid;
	can_msg = { 0 };
	dbc_msg_hdr_t msg_hdr = dbc_encode_COM_BRIDGE_HEARTBEAT(can_msg.data.bytes, &hb);
	can_msg.msg_id = msg_hdr.mid;
	can_msg.frame_fields.data_len = msg_hdr.dlc;
	CAN_tx(can1, &can_msg, 0);

	BT(p);
}

void period_10Hz(uint32_t count)
{
    // TODO : Jon : Is while loop only way here? If you synchronize your messages
    //              and everyone sends at 10Hz, then you only need to poll once
    //              and continue on. You won't need the while loop here if you do so.

	while(CAN_rx(can1, &can_msg, 0))
	{
		dbc_msg_hdr_t can_msg_hdr;
		can_msg_hdr.dlc = can_msg.frame_fields.data_len;
		can_msg_hdr.mid = can_msg.msg_id;
		if(can_msg_hdr.mid == GPS_CURRENT_LOCATION_HDR.mid)
			dbc_decode_GPS_CURRENT_LOCATION(&gpsData, can_msg.data.bytes, &can_msg_hdr);
		// TODO : Jon : else if ?
		if(can_msg_hdr.mid == GPS_ACKNOWLEDGEMENT_HDR.mid)
		{
			dbc_decode_GPS_ACKNOWLEDGEMENT(&gpsACK, can_msg.data.bytes, &can_msg_hdr);
			isACK = true;
		}
	}

	dbc_handle_mia_GPS_CURRENT_LOCATION(&gpsData, 100);
	dbc_handle_mia_GPS_ACKNOWLEDGEMENT(&gpsACK, 100);

	// TODO : Jon : if (isInitialLocation && gpsData.GPS_LATTITUDE_SIGNED != 0)
	//              You dont need to compare to false
	if(isInitialLocation == false && gpsData.GPS_LATTITUDE_SIGNED != 0)
	{
		printf(" Current Location = %f : %f\n", gpsData.GPS_LATTITUDE_SIGNED, gpsData.GPS_LONGITUDE_SIGNED );
		isInitialLocation = true;
	}
	else
	{
	  // TODO : Jon : Same here --> if (isClockEnabled)
	  //              Get rid of the true comparison
	  if(isClickEnabled == true)
	  {
		if(count1 < 0 && gpsACK.GPS_ACKNOWLEDGEMENT_UNSIGNED == 100)
			count1 = setCount;
		if(gpsACK.GPS_ACKNOWLEDGEMENT_UNSIGNED != 0 && count1 >= 0 and !isACK)
		{
			 printf("Count : %d\n", count1);
			 m.m0.COM_BRIDGE_TOTAL_COUNT_UNSIGNED = setCount;
			 m.m0.COM_BRIDGE_CURRENT_COUNT_UNSIGNED = count1;
			 m.m0.COM_BRIDGE_LATTITUDE_SIGNED = latitude[count1];
			 can_msg = { 0 };
			 dbc_msg_hdr_t msg_hdr = dbc_encode_COM_BRIDGE_CHECK_POINT_m0(can_msg.data.bytes,&m.m0);
			 can_msg.msg_id = msg_hdr.mid;
			 can_msg.frame_fields.data_len = msg_hdr.dlc;
			 CAN_tx(can1, &can_msg, 0);

			 m.m1.COM_BRIDGE_TOTAL_COUNT_UNSIGNED = setCount;
			 m.m1.COM_BRIDGE_CURRENT_COUNT_UNSIGNED = count1;
			 m.m1.COM_BRIDGE_LONGITUDE_SIGNED = longitude[count1];
			 can_msg = { 0 };
			 msg_hdr = dbc_encode_COM_BRIDGE_CHECK_POINT_m1(can_msg.data.bytes,&m.m1);
			 can_msg.msg_id = msg_hdr.mid;
			 can_msg.frame_fields.data_len = msg_hdr.dlc;
			 CAN_tx(can1, &can_msg, 0);
			 count1--;
		}
		else
		{
			if(startSig.COM_BRIDGE_CLICKED_START_UNSIGNED)
			{
			 can_msg = { 0 };
			 dbc_msg_hdr_t msg_hdr = dbc_encode_COM_BRIDGE_CLICKED_START(can_msg.data.bytes,&startSig);
			 can_msg.msg_id = msg_hdr.mid;
			 can_msg.frame_fields.data_len = msg_hdr.dlc;
			 CAN_tx(can1, &can_msg, 0);
			 startSig.COM_BRIDGE_CLICKED_START_UNSIGNED = { 0 };
			}

			//Continue accepting current location and sending to android
			//printf(" Moving CAR Location = %f : %f\n", gpsData.GPS_LATTITUDE_SIGNED, gpsData.GPS_LONGITUDE_SIGNED );
		}
	   }
	 else
	 {
		   if(isStopEnabled)
		   {
		    // TODO : Jon : Modify or make a wrapper for the CAN_tx()
		    //              function so where you can just say CAN_tx_send_stopsig(msg_hrx.mid, &stopSig);
		    //              and the function will declare the can_msg instance locally, so that you don't
		    //              have to keep declaring a can_msg multiple times throughout this file.
		    //              Do this for all of your can messages
			can_msg = { 0 };
			stopSig.COM_BRIDGE_STOPALL_UNSIGNED = COM_BRIDGE_STOPALL_HDR.mid;
			dbc_msg_hdr_t msg_hdr = dbc_encode_COM_BRIDGE_STOPALL(can_msg.data.bytes,&stopSig);
			can_msg.msg_id = msg_hdr.mid;
			can_msg.frame_fields.data_len = msg_hdr.dlc;
			CAN_tx(can1, &can_msg, 0);
		   }
	 }
   }
}

void period_100Hz(uint32_t count)
{
    // TODO : Jon : Single static functions i.e. handle_leds();
	if(isClickEnabled)
	{
		LE.on(3);
		LE.off(4);
	}
	else if(isStopEnabled)
	{
		LE.on(4);
		LE.off(3);
	}

	if(SW.getSwitch(3))
	{
		u3->putline("Nikhil#");
	}
}

// 1Khz (1ms) is only run if Periodic Dispatcher was configured to run it at main():
// scheduler_add_task(new periodicSchedulerTask(run_1Khz = true));
void period_1000Hz(uint32_t count)
{
    //LE.toggle(4);
}

void setupBT()
{
	int i = 0;
	delay_ms(3000);
	while(1)
	{
		i++;

		if(i >= 3)
		{
			printf("BTSETUP");
			i = 0;
			break;
		}

	}
}

void BT(void *p)
{
	string temp = "";
	int i = 0;
	char c;
	while(u3->getChar(&c, 100))
	{
		string temp = " ";
		temp[0] = c;
		if(c == 'B')
		{
			isClickEnabled = true;
			startSig.COM_BRIDGE_CLICKED_START_UNSIGNED = COM_BRIDGE_CLICKED_START_HDR.mid;
			count1 = setCount;
			isStopEnabled = false;
		}
		else if(c == 'E')
		{
			isClickEnabled = false;
			isStopEnabled = true;
			setCount = 0;
		}
		else if(c == '#')
		{
			cout << "Latitude :" << Latlng << endl;
			istringstream(Latlng) >> latitude[i];
			cout << latitude[i] << endl;
			Latlng = "";
		}
		else if(c == '$')
		{
			cout << "Longitude :" << Latlng << endl;
			istringstream(Latlng) >> longitude[i];
			cout << longitude[i] << endl;
			Latlng = "";
			i++;
			setCount++;
		}
		else
		{
			Latlng = Latlng + temp;
		}
	}
}

