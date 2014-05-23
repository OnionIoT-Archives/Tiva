#ifndef ONION_PAYLOAD_DATA_H
#define ONION_PAYLOAD_DATA_H

#include "OnionPacket.hpp"
#include <stdint.h>


class OnionPayloadData {
public:
	OnionPayloadData();
	OnionPayloadData(OnionPacket* pkt);
	OnionPayloadData(OnionPacket* pkt,uint16_t offset);
	
	~OnionPayloadData();
	// Initalize data in object
	void init(OnionPacket* pkt,uint16_t offset);
	// Call unpack to try and unpack the data into link list of payload objects
	uint16_t unpack(void);
	// getItem(item) will return an item from an array or map so you can dig into the data structure.
	// item is 0 based index of the array, i should be less than Length (from getLength() below)
	OnionPayloadData* getItem(uint16_t i);
	// getType() will return the type of this object
	uint8_t getType(void);
	// getLength() will return the number of items in an array or map, will return 1 for other data
	// and 0 for any data that could not be parsed correctly.
	uint16_t getLength(void);
	// getBuffer() will return a uint8_t* pointer to the data parse in the object. Useful for
	// getting a pointer to a string/binary/raw data type
	uint8_t* getBuffer(void);
	// ???  Should I add a getString to make sure the data has a null terminated string? Might be nice to be able to
	// ???  pass an output directly to str functions
	// getInt() will return the raw parse data as an int.  If the type is not int it will return 0
	int16_t getInt(void);
	// getBool() will return the parsed data of a bool, or false if it is another type
	bool getBool(void);
	int16_t getRawLength();
protected:
    // Internal use for unpacking
    uint16_t unpackArray(uint16_t bytesParsed,uint16_t length);
	uint16_t unpackMap(uint16_t bytesParsed,uint16_t length);
//    void unpackInt(void);
	uint16_t unpackStr(uint8_t* raw,uint16_t length);
//	void unpackNil(void);
//	void unpackBool(void);

private:
	OnionPacket* pkt;
	uint16_t offset;
	uint8_t* rawBuffer;
	uint16_t rawLength;
	uint8_t type;
	uint16_t length;
	// Data values
	void* data;
	OnionPayloadData **dataObjectArray;
	bool dataIsObject;
	bool dataIsMap;
};

#endif
