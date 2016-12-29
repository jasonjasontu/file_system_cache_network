////////////////////////////////////////////////////////////////////////////////
//
//  File          : cart_client.c
//  Description   : This is the client side of the CART communication protocol.
//
//   Author       : Jason Jincheng Tu
//  Last Modified : Dec 8th 2016
//

// Include Files
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

// Project Include Files
#include <cart_network.h>
#include <cmpsc311_log.h>
#include <cart_controller.h>
#include <cart_cache.h>
#include <cart_driver.h>
#include <cmpsc311_util.h>

//
//  Global data
int                cart_network_shutdown = 0;   // Flag indicating shutdown
char     	  *cart_network_address = NULL; // Address of CART server
unsigned short     cart_network_port = 0;       // Port of CART serve
unsigned long      CartControllerLLevel = 0; // Controller log level (global)
unsigned long      CartDriverLLevel = 0;     // Driver log level (global)
unsigned long      CartSimulatorLLevel = 0;  // Driver log level (global)

int 			socket_handle = -1;	//for socket id
struct sockaddr_in 	caddr;			//for address making
CartXferRegister 	buffer_reg;		//for change reg to network format

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : client_cart_bus_request
// Description  : This the client operation that sends a request to the CART
//                server process.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : reg - the request reqisters for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed

CartXferRegister client_cart_bus_request(CartXferRegister reg, void *buf) {
	uint64_t result;

	if (cart_network_shutdown == 0) {

		caddr.sin_family = AF_INET;
		//Setup the address
		if (cart_network_port == 0){
			cart_network_port = CART_DEFAULT_PORT;
		}
		caddr.sin_port = htons(cart_network_port);

		if (cart_network_address == NULL){
			cart_network_address = CART_DEFAULT_IP;
		}

		if ( inet_aton(cart_network_address, &caddr.sin_addr) == 0 ) {
			logMessage(LOG_OUTPUT_LEVEL, "Error: inet_aton");
 			return( -1 );
		}

		//Create the socket
		socket_handle = socket(PF_INET, SOCK_STREAM, 0);
		if (socket_handle == -1){
			logMessage(LOG_OUTPUT_LEVEL, "Error: socket create");
			return( -1 );
		}

		//Create the connection
		if ( connect(socket_handle, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1 ) {
			logMessage(LOG_OUTPUT_LEVEL, "Error: connection issue");

			return( -1 );
		} 
		cart_network_shutdown =1;
	}

	uint64_t OpCodes = reg >> 56;
	buffer_reg = htonll64(reg);

	if (OpCodes == CART_OP_RDFRME){
		write(socket_handle, &buffer_reg, sizeof(buffer_reg));	
		read(socket_handle, &result, sizeof(result));	
		read(socket_handle, buf, 1024);
	}
	else if(OpCodes == CART_OP_WRFRME){
		write(socket_handle, &buffer_reg, sizeof(buffer_reg));
		write(socket_handle, buf, 1024);		
		read(socket_handle, &result, sizeof(result));
	}
	else if(OpCodes == CART_OP_POWOFF){
		write(socket_handle, &buffer_reg, sizeof(buffer_reg));
		read(socket_handle, &result, sizeof(result));
		//cloce connection
		close( socket_handle );
		cart_network_shutdown = 0;
	}
	else{
		write(socket_handle, &buffer_reg, sizeof(buffer_reg));	
		read(socket_handle, &result, sizeof(result));	
	}
	return result;
}






