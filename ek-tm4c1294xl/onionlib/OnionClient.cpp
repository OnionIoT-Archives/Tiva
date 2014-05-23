#include "OnionClient.hpp"
#include "msgpack_types.h"
#include "OnionPacket.hpp"
#include "OnionPayloadData.hpp"
#include "OnionPayloadPacker.hpp"
#include "OnionParams.hpp"
//#include "OnionInterface.h"
#include <stdint.h>
#include <string.h>

#include "inc/hw_memmap.h"
#include "driverlib/rom_map.h"
#include "driverlib/flash.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "utils/uartstdio.h"
#include "utils/lwiplib.h"
#include "drivers/eth_client_lwip.h"



volatile uint32_t millis = 0;
bool available = false;
char OnionClient::domain[] = "device.onion.io";
uint16_t OnionClient::port = 2721;
bool volatile OnionClient::socketConnected = false;
OnionPacket* OnionClient::recvPkt = 0;

//static char* publishMap[] = {"ipAddr","192.168.137.1","mac","deadbeef"};
OnionClient::OnionClient(char* deviceId, char* deviceKey) {
    this->deviceId = new char[strlen(deviceId) + 1];
    this->deviceId[0] = 0;
    strcpy(this->deviceId, deviceId);

    this->deviceKey = new char[strlen(deviceKey) + 1];
    this->deviceKey[0] = 0;
    strcpy(this->deviceKey, deviceKey);
	
    this->remoteFunctions = new remoteFunction[1];
    this->remoteFunctions[0] = NULL;
    this->totalFunctions = 1;
    this->lastSubscription = NULL;
    totalSubscriptions = 0;
    this->isOnline = false;
    this->isConnected = false;
    recvPkt = new OnionPacket(1500);
}


void OnionClient::begin() {
	if (connect()) {
	}
}

bool OnionClient::connect() {
//    if (_client == 0) {
//        return false;
//    }
	if (!isOnline) {
//	    if (_client->connected()) {
//	        close();
//	    }
		//int result = open();
		if (socketConnected) {
            OnionPacket* pkt = new OnionPacket(ONION_MAX_PACKET_SIZE);
            pkt->setType(ONIONCONNECT);
            OnionPayloadPacker* pack = new OnionPayloadPacker(pkt);
            pack->packArray(3);
            pack->packInt(ONIONPROTOCOLVERSION);
            pack->packStr(deviceId);
            pack->packStr(deviceKey);
            send(pkt);
    		lastInActivity = lastOutActivity = millis;
            delete pack;
            isConnected = true;
            pingOutstanding = false;
            return true;
		}
		//close();
	}
	return false;
}

char* OnionClient::registerFunction(char* endpoint, remoteFunction function) {
    return registerFunction(endpoint,function,0,0);
}

char* OnionClient::registerFunction(char * endpoint, remoteFunction function, char** params, uint8_t param_count) {
	remoteFunction* resized = new remoteFunction[totalFunctions + 1];
	if (lastSubscription == NULL) {
	    subscriptions.id = totalFunctions;
	    subscriptions.endpoint = endpoint;
	    subscriptions.params = params;
	    subscriptions.param_count = param_count;
	    lastSubscription = &subscriptions;
	} else {
	    subscription_t* new_sub = new subscription_t;
	    new_sub->id = totalFunctions;
	    new_sub->endpoint = endpoint;
	    new_sub->params = params;
	    new_sub->param_count = param_count;
	    new_sub->next = NULL;
	    lastSubscription->next = new_sub;
	    lastSubscription = new_sub; 
	}
	totalSubscriptions++;
	
	for (int i = 0; i < totalFunctions; i++) {
		resized[i] = remoteFunctions[i];
	}
	
	// Set the last element of resized as the new function
	resized[totalFunctions] = function;
	
	delete [] remoteFunctions;
	remoteFunctions = resized;
	totalFunctions++;
	
	return 0;
	
//	char* idStr = new char[6];
//	idStr[0] = 0;
//	sprintf(idStr, "%d", totalFunctions);
//
//	return idStr;
};


bool OnionClient::publish(char* key, char* value) {
	int key_len = strlen(key);
	int value_len = strlen(value);
	if (isOnline) {
        OnionPacket* pkt = new OnionPacket(ONION_MAX_PACKET_SIZE);
        pkt->setType(ONIONPUBLISH);
        OnionPayloadPacker* pack = new OnionPayloadPacker(pkt);
        pack->packMap(1);
        pack->packStr(key);
        pack->packStr(value);
        
	    send(pkt);
        //pkt->send();
        delete pack;
        //delete pkt;
        return true;
	}
	return false;
}

bool OnionClient::publish(char** dataMap, uint8_t count) {
    if (isOnline) {
        OnionPacket* pkt = new OnionPacket(ONION_MAX_PACKET_SIZE);
        pkt->setType(ONIONPUBLISH);
        OnionPayloadPacker* pack = new OnionPayloadPacker(pkt);
        pack->packMap(count);
        for (uint8_t x=0; x<count; x++) {
            pack->packStr(*dataMap++);
            pack->packStr(*dataMap++);
        }
        send(pkt);
        delete pack;
        return true;
    }
    return false;
}

bool OnionClient::subscribe() {
	if (isConnected) {
	    // Generate 
	    if (totalSubscriptions > 0) {
            OnionPacket* pkt = new OnionPacket(ONION_MAX_PACKET_SIZE);
            pkt->setType(ONIONSUBSCRIBE);
            OnionPayloadPacker* pack = new OnionPayloadPacker(pkt);
	        subscription_t *sub_ptr = &subscriptions;
	        pack->packArray(totalSubscriptions);
        	//uint8_t string_len = 0;
        	uint8_t param_count = 0;
	        for (uint8_t i=0;i<totalSubscriptions;i++) {
	            param_count = sub_ptr->param_count;
	            pack->packArray(param_count+2);
	            pack->packStr(sub_ptr->endpoint);
	            pack->packInt(sub_ptr->id);
	            for (uint8_t j=0;j<param_count;j++) {
	                pack->packStr(sub_ptr->params[j]);
	            }
	            sub_ptr = sub_ptr->next;
	        }
	        send(pkt);
	        delete pack;
	        return true;
	    }
	    
	}
	return false;
}

bool OnionClient::loop() {
	if (isConnected) {
		unsigned long t = millis;
		if ((t - lastInActivity > ONION_KEEPALIVE * 1000UL) || (t - lastOutActivity > ONION_KEEPALIVE * 1000UL)) {
			if (pingOutstanding) {
				close();
				return false;
			} else {
			    sendPingRequest();
				lastOutActivity = t;
				lastInActivity = t;
				pingOutstanding = true;
			}
		}
        //OnionPacket* pkt = getPacket();
		if (available) {
		    
			lastInActivity = t;
			uint8_t type = recvPkt->getType();
			if (type == ONIONCONNACK) {
			    if (recvPkt->getPayload()[0] == 0) {
    			    subscribe();
    			} else {
    			    close();
                            available = false;
    			    //delete pkt;
    			    return false;
    			}

		    } else if (type == ONIONSUBACK) {
				isOnline = true;
        		//publish(publishMap,2);
				lastOutActivity = t;
			} else if (type == ONIONPUBLISH) {
			    parsePublishData(recvPkt);
			} else if (type == ONIONPINGREQ) {
			    // Functionize this
				sendPingResponse();
				lastOutActivity = t;
			} else if (type == ONIONPINGRESP) {
				pingOutstanding = false;
			} 
	        available = false;
			//delete pkt;
		}
		return true;
	} else {
	    // not connected
	    unsigned long t = millis;
	    if (socketConnected) {
    		if (t - lastOutActivity > ONION_RETRY * 1000UL) {
    		    this->begin();
    	    }
    	}
	    return true;
	}
}


void OnionClient::sendPingRequest(void) {
    OnionPacket* pkt = new OnionPacket(8);
    pkt->setType(ONIONPINGREQ);
    send(pkt);
}

void OnionClient::sendPingResponse(void) {
    OnionPacket* pkt = new OnionPacket(8);
    pkt->setType(ONIONPINGRESP);
    send(pkt);
}

void OnionClient::parsePublishData(OnionPacket* pkt) {
    
    uint16_t length = pkt->getBufferLength();
    uint8_t *ptr = pkt->getBuffer();
    OnionPayloadData* data = new OnionPayloadData(pkt);
    
    data->unpack();
    uint8_t count = data->getLength();
    uint8_t function_id = data->getItem(0)->getInt();
//	OnionParams* params = new OnionParams(count-1);
    char **params = 0;
    if (count > 1) {
        params = new char*[count-1];
	    // Get parameters
	    for (uint8_t i=0;i<(count-1);i++) {
	        OnionPayloadData* item = data->getItem(i+1);
	        uint8_t strLen = item->getLength();
	        // Test
	        params[i] = (char *)(item->getBuffer());
	        //params->setStr(i,buf_ptr,strLen);
	    }
	}
	if (function_id < totalFunctions) {
	    if (remoteFunctions[function_id] != 0) {
	        remoteFunctions[function_id](params);
	    }
	} 
	if (params != 0) {
	    delete[] params;   
	}
	delete data;
}


bool OnionClient::open() {
    // If we don't already have a socket open, then open one
    //if () {
		//return _client->connect(OnionClient::domain, OnionClient::port);
	//if (EthClientTCPConnect() == 0) {
	    return true;
	//} else {
	//    return false;
	//}
	//}
	// if we already had an open socket then let's say we're good
	//return 1;
}

int8_t OnionClient::send(OnionPacket* pkt) {
        int length = pkt->getBufferLength();
        // Send packet
        //int rc = _client->write((uint8_t*)pkt->getBuffer(), length);
        EthClientSend((int8_t*)pkt->getBuffer(),length);
        // Report if packet was sent successfully
        // for now we'll destroy the packet either way, but maybe we should be smarter
        //if (length == rc) {
            delete pkt;
            return 1;
        //}
        delete pkt;
        return 0;
}


// This should probably be "handlePacket" since it's going to be called once a packet is recieved
OnionPacket* OnionClient::getPacket(void) {
    uint16_t count = 0;//_client->available();
    if (count > 0) {
        if (recvPkt == 0) {
            recvPkt = new OnionPacket(ONION_MAX_PACKET_SIZE);
        }
        
        if (count > recvPkt->getFreeBuffer()) {
            // Clear existing buffer?  This could be leading to problems in Arduino
            //_client->flush();
            delete recvPkt;
            recvPkt = 0;
            return 0;
        }
        uint8_t *ptr = recvPkt->getPtr();
        for (uint16_t x = 0;x<count;x++) {
            // We shoudn't need to do this with lwip
            //*ptr++ = _client->read();
        }
        recvPkt->incrementPtr(count);
        uint8_t type = recvPkt->getType();
        if (recvPkt->isComplete()) {
            OnionPacket* pkt = recvPkt;
            recvPkt = 0;
            return pkt;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

void OnionClient::close(void) {
    //_client->flush();
    //_client->stop();
    EthClientTCPDisconnect();
    isOnline = false;
    isConnected = false;
}

//// Specific implementation for TI


void EthernetCallback(uint32_t ui32Event, void *pvData, uint32_t ui32Param) {
    uint8_t *pD = (uint8_t *)pvData;

    //
    // Handle events from the Ethernet client layer.
    //
    switch(ui32Event)
    {
        //
        // Ethernet client has received data.
        //
        case ETH_CLIENT_EVENT_RECEIVE:
        {
            // handle packet recieve parsing pD is data ui32Param is length
            memcpy(OnionClient::recvPkt->getBuffer(),pD,ui32Param);
            OnionClient::recvPkt->setPayloadLength(ui32Param-3);
            available = true;
        }

        //
        // Ethernet client has connected to the specified server/host and port.
        //
        case ETH_CLIENT_EVENT_CONNECT:
        {
            // update state of connection
	        OnionClient::socketConnected = true;
            break;
        }
        //
        // Ethernet client has obtained IP address via DHCP.
        //
        case ETH_CLIENT_EVENT_DHCP:
        {
            EthClientDNSResolve();
            break;
        }

        //
        // Ethernet client has received DNS response.
        //
        case ETH_CLIENT_EVENT_DNS:
        {
            if(ui32Param != 0)
            {
                //
                // If DNS resolved successfully, initialize the socket and
                // stack.
                //
                EthClientTCPConnect();
            }
            else
            {
                // Report error / update state (no dns available)
            }

            break;
        }
        //
        // Ethernet client has disconnected from the server/host.
        //
        case ETH_CLIENT_EVENT_DISCONNECT:
        {
            //
            // Close the socket.
            //
	        OnionClient::socketConnected = false;
            EthClientTCPDisconnect();

            break;
        }

        //
        // All other cases are unhandled.
        //
        case ETH_CLIENT_EVENT_SEND:
        {

            break;
        }


        case ETH_CLIENT_EVENT_ERROR:
        {
            
            break;
        }

        default:
        {

            break;
        }

    }
}

void OnionClient::init(uint32_t sys_clock_hz) {    //
    
    //lwIPInit(sys_clock_hz, pui8MACArray, 0, 0, 0, IPADDR_USE_DHCP);
    EthClientInit(sys_clock_hz,&EthernetCallback);
    EthClientHostSet(OnionClient::domain,OnionClient::port);
}

void OnionClient::systick(uint32_t ms_period) {
    
    //
    // Call the lwIP timer handler.
    //
    //lwIPTimer(ms_period);
    EthClientTick(ms_period);
    millis += ms_period;
}



//*****************************************************************************
//
// The current IP address.
//
//*****************************************************************************
uint32_t g_ui32IPAddress;

//*****************************************************************************
//
// Required by lwIP library to support any host-related timer functions.
//
//*****************************************************************************
void
lwIPHostTimerHandler(void)
{
    uint32_t ui32Idx, ui32NewIPAddress;

    //
    // Get the current IP address.
    //
    ui32NewIPAddress = lwIPLocalIPAddrGet();

    //
    // See if the IP address has changed.
    //
    if(ui32NewIPAddress != g_ui32IPAddress)
    {
        //
        // See if there is an IP address assigned.
        //
        if(ui32NewIPAddress == 0xffffffff)
        {
            //
            // Indicate that there is no link.
            //
            UARTprintf("Waiting for link.\n");
        }
        else if(ui32NewIPAddress == 0)
        {
            //
            // There is no IP address, so indicate that the DHCP process is
            // running.
            //
            UARTprintf("Waiting for IP address.\n");
        }
        else
        {
            //
            // Display the new IP address.
            //
            //UARTprintf("IP Address: ");
            //DisplayIPAddress(ui32NewIPAddress);
            //UARTprintf("\nOpen a browser and enter the IP address.\n");
        }

        //
        // Save the new IP address.
        //
        g_ui32IPAddress = ui32NewIPAddress;

        //
        // Turn GPIO off.
        //
        MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, ~GPIO_PIN_1);
    }

    //
    // If there is not an IP address.
    //
    if((ui32NewIPAddress == 0) || (ui32NewIPAddress == 0xffffffff))
    {
        //
        // Loop through the LED animation.
        //

        for(ui32Idx = 1; ui32Idx < 17; ui32Idx++)
        {

            //
            // Toggle the GPIO
            //
            MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1,
                    (MAP_GPIOPinRead(GPIO_PORTN_BASE, GPIO_PIN_1) ^
                     GPIO_PIN_1));

            SysCtlDelay(120000000/(ui32Idx << 1));
        }
    }
}



