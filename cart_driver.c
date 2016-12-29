////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the CRUD storage system.
//
//  Author         : [Jason Jincheng Tu]
//  Last Modified  : []
//

// Includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Project Includes
#include <cart_driver.h>
#include <cart_controller.h>
#include <cart_cache.h>
#include <cmpsc311_log.h>
#include <cart_network.h>

//
// Implementation

//declare all the variable used in the drive

// for file mapping between a file handle and corresponding frame.
struct File{
	int position;
	uint16_t Cartridge;
	uint16_t Frame;
	int ending_position;
	uint16_t ending_cartridge;
	uint16_t ending_frame;
	char fileName[CART_MAX_PATH_LENGTH];
} FileList[CART_MAX_TOTAL_FILES];

//buffer for read and write function
char buffer[1024];

////////////////////////////////////////////////////////////////////////////////
//
// Function     : locate_empty_frame
// Description  : get a new empty frame for a file
//
// Inputs       : file id
// Outputs      : 0 if successful, 1 if failure

int locate_empty_frame(uint16_t fd){
	for (int i = FileList[fd].ending_cartridge; i < CART_MAX_CARTRIDGES; i++){
		for (int j = FileList[fd].ending_frame+1; j < CART_CARTRIDGE_SIZE; j++){
			if (CartridgeMap[i][j] == 0){
				CartridgeMap[i][j] = fd;
				FileList[fd].Cartridge = i;
				FileList[fd].Frame = j;
				FileList[fd].position = 0;
				FileList[fd].ending_position = 0;
				FileList[fd].ending_cartridge = i;
				FileList[fd].ending_frame = j;
				return (0);	//return successful
			}
		}
		FileList[fd].ending_frame = 0;
	}
	return (1);//return fail
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : find_next_frame
// Description  : find the next frame that belongs to a file
//
// Inputs       : file id
// Outputs      : 0 if successful, -1 if failure, 1 if it is already the end of the file

//for locate the next frame for this file, return 1 if this is the last frame, return 0 if it is find, return -1 for error

int find_next_frame(int16_t fd){
	if(FileList[fd].Cartridge == FileList[fd].ending_cartridge && FileList[fd].Frame == FileList[fd].ending_frame){
		return(1);
	}
	for (int i = FileList[fd].Cartridge; i < CART_MAX_CARTRIDGES; i++){
		for (int j = FileList[fd].Frame+1; j < CART_CARTRIDGE_SIZE; j++){
			if (CartridgeMap[i][j] == fd){
				FileList[fd].Cartridge = i;
				FileList[fd].Frame = j;
				FileList[fd].position = 0;
				return(0);
			}
		}
		FileList[fd].Frame = 0;
	}
	return(-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweron
// Description  : Startup up the CART interface, initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful

int32_t cart_poweron(void) {

	CartXferRegister cart = make_cart(CART_OP_INITMS,0,0,0);
	client_cart_bus_request(cart, NULL);

	uint16_t Cartriage = 0;
	
	//load and bzero all the cartridge
	while(Cartriage < 64){
		
		cart = make_cart(CART_OP_LDCART,0,Cartriage,0);
		client_cart_bus_request(cart, NULL);

		cart = make_cart(CART_OP_BZERO,0,Cartriage,0);
		client_cart_bus_request(cart, NULL);

		Cartriage++;
	}
	
	strncpy(FileList[0].fileName,"Reserved", 10);//the first spot in file map is reserved for programming purpose

	//initial the file map	
	for(int i=1;i<1024;i++){
		FileList[i].fileName[0]='\0';
	}

	//initial the cartridge map
	for (int i = 0; i < CART_MAX_CARTRIDGES; i++){
		for (int j = 0; j < CART_CARTRIDGE_SIZE; j++){
			CartridgeMap[i][j] = 0;
		}
	}

	//initial cache
	init_cart_cache();

	//initial socket
	client_socket = -1;

	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweroff
// Description  : Shut down the CART interface, close all files
//
// Inputs       : none
// Outputs      : 0 if successful

int32_t cart_poweroff(void) {
	//close cache
	close_cart_cache();

	//clean the filelist
	for(int i = 0; i < 20; i++){
		FileList[i].fileName[0] = '\0';
	}

	//clean the mapping
	for(int c = 0; c < 64; c++){
		for(int f = 0; f<1024;f++){
			CartridgeMap[c][f] = 0;
		}
	}

	CartXferRegister cart = make_cart(CART_OP_POWOFF,0,0,0);
	client_cart_bus_request(cart, NULL);

	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle

int16_t cart_open(char *path) {
	int16_t i=0;
	while(FileList[i].fileName[0] != '\0'){
		i++;	
	}
	
	strncpy(FileList[i].fileName, path, 128);
	
	FileList[i].ending_cartridge = 0;
	FileList[i].ending_frame = 0;
	locate_empty_frame(i);
	
	//RETURN A FILE HANDLE
	return (i);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful

int16_t cart_close(int16_t fd) {

	FileList[fd].fileName[0] = '\0';

	for(int c = 0; c < 64; c++){
		for(int f = 0; f<1024;f++){
			if (CartridgeMap[c][f] == fd){
				CartridgeMap[c][f] = 0;
			}
		}
	}

	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : reading
// Description  : use to call cache for data
//
// Inputs       : file id and buf for the data
// Outputs      : none

void reading(int16_t fd, char *buf){
	
	char *b = (char *) get_cart_cache(FileList[fd].Cartridge,FileList[fd].Frame);
	memcpy(buf, b, 1024);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_read
// Description  : Reads "count" bytes from the file handle "fd" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read

int32_t cart_read(int16_t fd, void *buf, int32_t count) {
	int32_t bits_read = 0;
	int32_t temp;

	if (FileList[fd].fileName[0] == '\0'){
		return (-1);
	}

	//loop through the file
	while(bits_read != count){
		temp = count - bits_read;

		//not enough bit in the file to read
		if (FileList[fd].Cartridge == FileList[fd].ending_cartridge 
		&& FileList[fd].Frame == FileList[fd].ending_frame 
		&& temp > (FileList[fd].ending_position - FileList[fd].position)){
			temp = FileList[fd].ending_position - FileList[fd].position;
			reading(fd, buffer);
			memcpy(&((char *)buf)[bits_read], &buffer[FileList[fd].position], temp);
			bits_read += temp;
			FileList[fd].position += temp;
			return (bits_read);
		}

		//read from the middle of the file
		if (FileList[fd].position != 0){
			temp = 1024 - FileList[fd].position;

			//on the first time, it does not read to the end of frame
			if (count < (1024 - FileList[fd].position)){
				temp = count;
			}
		}

		//keep it within a frame
		if (temp > 1024){			
			temp = 1024;
		}

		reading(fd, buffer);
		memcpy(&((char *)buf)[bits_read], &buffer[FileList[fd].position], temp);
		bits_read += temp;
		FileList[fd].position += temp;

		//if the frame is full, find a new frame in this file
		if (FileList[fd].position == 1024){
			if (find_next_frame(fd)==1){
				return (bits_read);
			}
		}		
	}

	// Return bits read
	return (bits_read);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : writing
// Description  : write the data in cache and the io bus
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
// Outputs      : none

void writing(int16_t fd, char *buf){	
	CartXferRegister c;
	//check to make sure that the correct cartridge is loaded
	if (FileList[fd].Cartridge != current_Cartridge){	
		c = make_cart(CART_OP_LDCART, 0, FileList[fd].Cartridge,FileList[fd].Frame);
		client_cart_bus_request(c, buf);
		current_Cartridge = FileList[fd].Cartridge;
	}

	c = make_cart(CART_OP_WRFRME,0,FileList[fd].Cartridge,FileList[fd].Frame);
	client_cart_bus_request(c, buf);

	put_cart_cache(FileList[fd].Cartridge, FileList[fd].Frame, buf);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written

int32_t cart_write(int16_t fd, void *buf, int32_t count) {	
	int32_t bits_written = 0;
	int32_t temp;

	if (FileList[fd].fileName[0] == '\0'){
		return (-1);
	}

	//loop through the rest of the file
	while(bits_written != count){
		//get the data for this position
		reading(fd, buffer);

		//set up value for writing
		temp = count - bits_written;

		//check the position of the file
		if (FileList[fd].position != 0){
			temp = 1024 - FileList[fd].position;
			if (count < temp){
				temp = count;
			}
		}

		if (temp > 1024){
			temp = 1024;
		}
		memcpy(&buffer[FileList[fd].position], &((char *)buf)[bits_written], temp);

		writing(fd, buffer);
		bits_written += temp;
		FileList[fd].position += temp;
		
		//change the length if nesserary 
		if (FileList[fd].Cartridge == FileList[fd].ending_cartridge 
		&& FileList[fd].Frame == FileList[fd].ending_frame){
			//if the frame is full, give a new frame to this file
			if (FileList[fd].position == 1024){
				locate_empty_frame(fd);
			}		
			else if ( (temp + FileList[fd].position) > FileList[fd].ending_position){
				FileList[fd].ending_position = temp + FileList[fd].position;
			}
		}

		//if the frame is full, give a new frame to this file
		else if (FileList[fd].position == 1024){
			find_next_frame(fd);
		}
	}
	
	// Return successfully
	return (bits_written);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_read
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful

int32_t cart_seek(int16_t fd, uint32_t loc) {

	int num_frame = (loc/1024) + 1;
	for (int i = 0; i < CART_MAX_CARTRIDGES; i++){
		for (int j = 0; j < CART_CARTRIDGE_SIZE; j++){
			if (CartridgeMap[i][j] == fd){
				num_frame -= 1;
				if (num_frame == 0){
					FileList[fd].position = loc;
					FileList[fd].Cartridge = i;
					FileList[fd].Frame = j;
				}
				else{
					loc -= 1024;
				}
			}
		}
	}

	// Return successfully
	return (0);
}
