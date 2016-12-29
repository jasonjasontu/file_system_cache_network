#ifndef CART_FRAME_CACHE_INCLUDED
#define CART_FRAME_CACHE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_frame_cache.h
//  Description    : This is the header file for the implementation of the
//                   frame cache for the cartridge memory system driver.
//
//  Author         : Patrick McDaniel and Jason Jincheng Tu
//  Last Modified  : 11/28/2016
//

// Includes
#include <cart_controller.h>

// Defines
#define DEFAULT_CART_FRAME_CACHE_SIZE 1024  // Default size for cache
// struct defined for link list for cache
typedef struct node{
	struct node * previous;
	struct node * next;
	uint16_t cart;
	uint16_t frm;
	char buffer[DEFAULT_CART_FRAME_CACHE_SIZE];
} node;

//map all the node in the link list to record the last use
typedef struct map{
	uint32_t last_use;
	uint16_t cart;
	uint16_t frm;
} map;

int16_t current_Cartridge;	// record the current cartridge number so cartridge will not reload

// Cache Interfaces

uint64_t make_cart(uint8_t KY1, uint8_t KY2, uint16_t CT1, uint16_t FT1);
	//make cart function shared by both cache and driver

int set_cart_cache_size(uint32_t max_frames);
	// Set the size of the cache (must be called before init)

int init_cart_cache(void);
	// Initialize the cache 

int close_cart_cache(void);
	// Clear all of the contents of the cache, cleanup

int put_cart_cache(CartridgeIndex cart, CartFrameIndex frm, void *frame);
	// Put an object into the object cache, evicting other items as necessary

void * get_cart_cache(CartridgeIndex dsk, CartFrameIndex blk);
	// Get an object from the cache (and return it)

//
// Unit test

int cartCacheUnitTest(void);
	// Run a UNIT test checking the cache implementation

#endif
