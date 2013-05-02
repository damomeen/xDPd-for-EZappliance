/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DATAPACKET_STORAGE_H_
#define DATAPACKET_STORAGE_H_

#include <vector>
#include <list>
#include <queue>
#include <cstddef>
#include <ctime>

#include <stdint.h>
#include <pthread.h>

#include <rofl/datapath/pipeline/common/datapacket.h>

/**
* @file datapacket_storage.h
* @author Tobias Jungel<tobias.jungel (at) bisdn.de>
* @author Victor Alvarez<victor.alvarez (at) bisdn.de>
*
* @brief Temporal storage for packets. 
*
*/

typedef uint32_t storeid;

class datapacket_storage
{
public:
	/**
	 * constructor
	 * @param size
	 */
	datapacket_storage(uint16_t size, uint16_t expiration_time_sec);

	/**
	 * destructor
	 */
	~datapacket_storage();
	
	/**
	 * store a datapacket
	 * @param pkt
	 * @return id of the stored packet, -1 if the packet could not be stored
	 */
	storeid
	store_packet(datapacket_t *pkt);

	/**
	 * get the stored packet by id
	 * @param id
	 * @return
	 */
	datapacket_t *
	get_packet(storeid id);

	/**
	 * get the size of the storage
	 * @return
	 */
	uint16_t
	get_storage_size() const;
	
	/**
	 * returns true if the first element needs to be
	 * expired, and sets it's ID in the parameter.
	 */
	bool
	oldest_packet_needs_expiration(storeid *id);
	
#ifdef DEBUG
	void change_expiration_time(uint16_t sec);
#endif

private:
	
	storeid next_id;
	uint16_t max_size;
	typedef struct {
		storeid id;
		datapacket_t* pkt;
		time_t input_timestamp;
	} store_mapping;

	std::list<store_mapping> store;
	pthread_mutex_t lock;
	uint16_t expiration_time_sec;

	// this class is noncopyable
	datapacket_storage(const datapacket_storage&);
	datapacket_storage& operator=(const datapacket_storage&);
};

#endif /* DATAPACKET_STORAGE_H_ */
