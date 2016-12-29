////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_cache.c
//  Description    : This is the implementation of the cache for the CART
//                   driver.
//
//  Author         : [Jason Jincheng Tu]
//  Last Modified  : [** YOUR DATE **]
//

// Includes

#include <stdlib.h>
#include <string.h> 
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


// Project includes
#include <cmpsc311_log.h>
#include <cart_controller.h>
#include <cart_cache.h>
#include <cart_driver.h>
#include <cart_network.h>


// Defines


node *top;

node *root;

uint32_t max = DEFAULT_CART_FRAME_CACHE_SIZE;

uint32_t size;

map *cache_map;

uint32_t use_counter;


//
// Functions


////////////////////////////////////////////////////////////////////////////////
//
// Function     : make_cart
// Description  : make a cart to talk to the io bus
//
// Inputs       : value for all parts of the cart
// Outputs      : a 64 bits number as the cart

uint64_t make_cart(uint8_t KY1, uint8_t KY2, uint16_t CT1, uint16_t FT1) {
	uint64_t ky1 = (uint64_t) KY1 << 56;	
	uint64_t ky2 = (uint64_t) KY2 << 48;
	uint64_t ct1 = (uint64_t) CT1 << 31;
	uint64_t ft1 = (uint64_t) FT1 << 15;
	uint64_t result = ky1 | ky2 | ct1 | ft1;
	return result;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : read
// Description  : getting data from the memory system 
//
// Inputs       : cart number, frame number, and a buf pointer for the data
// Outputs      : none

void reads(uint16_t cart, uint16_t frm, char *buf){
	CartXferRegister c;
	
	//check to make sure that the correct cartridge is loaded
	if (cart != current_Cartridge){	
		c = make_cart(CART_OP_LDCART, 0, cart, frm);
		client_cart_bus_request(c, buf);
		current_Cartridge = cart;
	}
	
	c = make_cart(CART_OP_RDFRME, 0, cart, frm);
	client_cart_bus_request(c, buf);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : update_map
// Description  : use for change the use counter when it reach to a number that is too big
//		change the value of the entire map for cache.
//
// Inputs       : none
// Outputs      : none

void update_map(){

	uint32_t use = size;
	uint32_t i = use_counter -1;
	bool *update = calloc(max, sizeof(bool));	//a mark for all items to prevent double updates

	while(use != 0){
		for(int j =0; j<size; j++){

			if ((!update[j]) && cache_map[j].last_use == i){
				cache_map[j].last_use = use - 1;
				use -= 1;
				update[j] = true;
				break;
			}
		}
		i -= 1;

	}

	//change the use counter at the end
	use_counter = size;
	
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : find buffer
// Description  : use for reading function to find the buffer of a given frame
//
// Inputs       : the information to find the frame (cart number and frame number)
// Outputs      : a pointer to the buffer of the frame

char *find_buffer(uint16_t cart, uint16_t frm){
	node *current = root;

	while(current != top){
		current = current->next;

		if(current->cart == cart && current->frm == frm){
			
			return current->buffer;
		}		
	}
	return NULL;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : set_cart_cache_size
// Description  : Set the size of the cache (must be called before init)
//
// Inputs       : max_frames - the maximum number of items your cache can hold
// Outputs      : 0 if successful, -1 if failure

int set_cart_cache_size(uint32_t max_frames) {
	max = max_frames;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : init_cart_cache
// Description  : Initialize the cache and note maximum frames
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int init_cart_cache(void) {

	cache_map = (map *) malloc(max*sizeof(map));		//need correction and change all to 0
	root = (node *) malloc(sizeof(node));
	top = root;
	root->previous = NULL;
	root->next = NULL;
	root->cart = 64;
	root->frm = 0;
	root->buffer[0] = '\0';
	use_counter = 1;
	cache_map[0].last_use = 0;	//the last use start with 0
	size = 0;
	current_Cartridge = 63;
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : delete_cart_cache
// Description  : Remove a frame from the cache (and return it)
//
// Inputs       : cart - the cart number of the frame to remove from cache
//                blk - the frame number of the frame to remove from cache
// Outputs      : pointe buffer inserted into the object


void * delete_cart_cache(CartridgeIndex cart, CartFrameIndex blk) {
	int i = 0;

	node *current = root->next;

	//loop through the link list find the one to delete
	while(current->cart != cart || current->frm != blk){
		i += 1;
		current = current->next;
	}

	//connect the link list back together
	current->previous->next = current->next;
	current->next->previous = current->previous;

	return current;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : new_node
// Description  : make a node in the cache link list
//
// Inputs       : the cart number and frame number of this new node
// Outputs      : a pointer for the buffer of that new node

char *new_node(uint16_t cart, uint16_t frm){

	uint32_t id; 
	id = size;

	//if the cache is full
	if (size == max){
		//find the one to drop
		uint32_t mini = use_counter;
		for(int j =0; j<max; j++){
			if (cache_map[j].last_use < mini){
				mini = cache_map[j].last_use;
				id = j;
			}
		}

		//delete the node in link list and write the frame back 
		node *temp = delete_cart_cache(cache_map[id].cart, cache_map[id].frm);

		free(temp);
		size -= 1;
	}

	top->next = (node *) malloc(sizeof(node));
	top->next->previous = top;
	
	top = top->next;
	top->next = NULL;
	top->cart = cart;
	top->frm = frm;
	reads(cart, frm, top->buffer);
	
	//change the map
	cache_map[id].frm = frm;
	cache_map[id].cart = cart;
	cache_map[id].last_use = use_counter;

	use_counter += 1;
	if (use_counter > (max*100)){
		update_map();
	}

	size += 1;

	return top->buffer;	
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : close_cart_cache
// Description  : Clear all of the contents of the cache, cleanup
//
// Inputs       : none
// Outputs      : o if successful, -1 if failure

int close_cart_cache(void) {

	free(root);
	free(cache_map);
	free(top);

	return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : put_cart_cache
// Description  : Put an object into the frame cache
//
// Inputs       : cart - the cartridge number of the frame to cache
//                frm - the frame number of the frame to cache
//                buf - the buffer to insert into the cache
// Outputs      : 0 if successful, -1 if failure

int put_cart_cache(CartridgeIndex cart, CartFrameIndex frm, void *buf)  {

	char *buffer = find_buffer(cart, frm);

	if(buffer == NULL){
		return (-1);
	}
	
	memcpy(buffer, (char *)buf, 1024);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_cart_cache
// Description  : Get an frame from the cache (and return it)
//
// Inputs       : cart - the cartridge number of the cartridge to find
//                frm - the  number of the frame to find
// Outputs      : pointer to cached frame or NULL if not found

void * get_cart_cache(CartridgeIndex cart, CartFrameIndex frm) {

	bool find = false;
	uint32_t i = 0;

	while (i < size){
		//update the map if we find the frame
		if(cache_map[i].cart == cart && cache_map[i].frm == frm){

			find = true;
			cache_map[i].last_use = use_counter;
			use_counter += 1;
			if (use_counter > (max*100)){
				update_map();
			}
			break;
		}
		i += 1;
	}

	//for the frame in the cache
	if (find){
		return find_buffer(cart, frm);
	}
	else{
		//for the frame not in the cache
		return new_node(cart, frm);
	}
}


// Unit test

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cartCacheUnitTest
// Description  : Run a UNIT test checking the cache implementation
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int cartCacheUnitTest(void) {	
	// Return successfully
	logMessage(LOG_OUTPUT_LEVEL, "Cache unit test completed successfully.");
	return(0);
}
