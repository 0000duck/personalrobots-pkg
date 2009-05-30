#include "pr2lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <net/if.h>

#include "host_netutil.h"
#include "ipcam_packet.h"

/// Amount of time in microseconds that the host should wait for packet replies
#define STD_REPLY_TIMEOUT SEC_TO_USEC(0.2)

#define VMODEDEF(width, height, fps) { #width"x"#height"x"#fps, width, height, fps }  
const struct MT9VMode MT9VModes[MT9V_NUM_MODES] = {
  VMODEDEF(752,480,15),
  VMODEDEF(752,480,12.5),
  VMODEDEF(640,480,30),
  VMODEDEF(640,480,25),
  VMODEDEF(640,480,15),
  VMODEDEF(640,480,12.5),
  VMODEDEF(320,240,60),
  VMODEDEF(320,240,50),
  VMODEDEF(320,240,30),
  VMODEDEF(320,240,25)
};

/**
 * Returns the version information for the library
 *
 *
 * @return	Returns the the library as an integer 0x0000MMNN, where MM = Major version and NN = Minor version
 */
int pr2LibVersion() {
	return PR2LIB_VERSION;
}


/**
 * Waits for a PR2 StatusPacket on the specified socket for a specified duration.
 *
 * The Status type and code will be reported back to the called via the 'type' & 'code'
 * arguments. If the timeout expires before a valid status packet is received, then
 * the function will still return zero but will indicate that a TIMEOUT error has occurred.
 *
 * @param s			The open, bound  & 'connected' socket to listen on
 * @param wait_us	The number of microseconds to wait before timing out
 * @param type		Points to the location to store the type of status packet
 * @param code		Points to the location to store the subtype/error code
 *
 * @return Returns 0 if no system errors occured. -1 with errno set otherwise.
 */
int pr2StatusWait( int s, uint32_t wait_us, uint32_t *type, uint32_t *code ) {
	if( !wgWaitForPacket(s, PKTT_STATUS, sizeof(PacketStatus), &wait_us) && (wait_us != 0) ) {
		PacketStatus sPkt;
		if( recvfrom( s, &sPkt, sizeof(PacketStatus), 0, NULL, NULL )  == -1 ) {
			perror("wgDiscover unable to receive from socket");
			*type = PKT_STATUST_ERROR;
			*code = PKT_ERROR_SYSERR;
			return -1;
		}

		*type = ntohl(sPkt.status_type);
		*code = ntohl(sPkt.status_code);
		return 0;
	}

	*type = PKT_STATUST_ERROR;
	*code = PKT_ERROR_TIMEOUT;
	return 0;
}


/**
 * Discovers all PR2 cameras that are connected to the 'ifName' ethernet interface and
 * adds new ones to the 'ipCamList' list.
 *
 * @param ifName 		The ethernet interface name to use. Null terminated string (e.g., "eth0").
 * @param ipCamList 	The list to which the new cameras should be added
 * @param wait_us		The number of microseconds to wait for replies
 *
 * @return	Returns -1 with errno set for system call errors. Otherwise returns number of new
 * 			cameras found.
 */
int pr2Discover(const char *ifName, IpCamList *ipCamList, const char *ipAddress, unsigned wait_us) {
	// Create and initialize a new Discover packet
	PacketDiscover dPkt;
	dPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	dPkt.hdr.type = htonl(PKTT_DISCOVER);
	strncpy(dPkt.hdr.hrt, "DISCOVER", sizeof(dPkt.hdr.hrt));

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(ifName, &dPkt.hdr.reply_to);
	if(s == -1) {
		perror("Unable to create socket\n");
		return -1;
	}
  
  //// Determine the broadcast address for ifName. This is the address we
  //// will tell the camera to send from.

	struct in_addr newIP;
	if (ipAddress) // An IP was specified
  {
    inet_aton(ipAddress, &newIP);
    dPkt.ip_addr = newIP.s_addr;
  }
  else // We guess an IP by flipping the host bits of the local IP.
  {
    struct in_addr localip;
    struct in_addr netmask;
    wgIpGetLocalAddr(ifName, &localip);
    wgIpGetLocalNetmask(ifName, &netmask);
    dPkt.ip_addr = localip.s_addr ^ netmask.s_addr ^ ~0;
  }

	if( wgSendUDPBcast(s, ifName, &dPkt,sizeof(dPkt)) == -1) {
		perror("Unable to send broadcast\n");
	}

	// For the old API
  if( wgSendUDPBcast(s, ifName, &dPkt,sizeof(dPkt)-sizeof(dPkt.ip_addr)) == -1) {
		perror("Unable to send broadcast\n");
	}

	// Count the number of new cameras found
	int newCamCount = 0;
	do {
		// Wait in the loop for replies. wait_us is updated each time through the loop.
		if( !wgWaitForPacket(s, PKTT_ANNOUNCE, sizeof(PacketAnnounce), &wait_us)  && (wait_us != 0) ) {
			// We've received an Announce packet, so pull it out of the receive queue
			PacketAnnounce aPkt;
      struct sockaddr_in fromaddr;
      fromaddr.sin_family = AF_INET;
      socklen_t fromlen = sizeof(fromaddr);

			if( recvfrom( s, &aPkt, sizeof(PacketAnnounce), 0, (struct sockaddr *) &fromaddr, &fromlen)  == -1 ) {
				perror("wgDiscover unable to receive from socket");
        close(s);
				return -1;
			}

			// Create a new list entry and initialize it
			IpCamList *tmpListItem;
			if( (tmpListItem = (IpCamList *)malloc(sizeof(IpCamList))) == NULL ) {
				perror("Malloc failed");
				close(s);
				return -1;
			}
			pr2CamListInit( tmpListItem );

			// Initialize the new list item's data fields (byte order corrected)
      tmpListItem->hw_version = ntohl(aPkt.hw_version);
      tmpListItem->fw_version = ntohl(aPkt.fw_version);
      tmpListItem->ip = fromaddr.sin_addr.s_addr;
			tmpListItem->serial = ntohl(aPkt.ser_no);
			memcpy(&tmpListItem->mac, aPkt.mac, sizeof(aPkt.mac));
			strncpy(tmpListItem->ifName, ifName, sizeof(tmpListItem->ifName));
			tmpListItem->status = CamStatusDiscovered;
			char pcb_rev = 0x0A + (0x0000000F & ntohl(aPkt.hw_version));
			int hdl_rev = 0x00000FFF & (ntohl(aPkt.hw_version)>>4);
		  snprintf(tmpListItem->hwinfo, PR2_CAMINFO_LEN, "PCB rev %X : HDL rev %3X : FW rev %3X", pcb_rev, hdl_rev, ntohl(aPkt.fw_version));

			// If this camera is already in the list, we don't want to add another copy
			if( pr2CamListAdd( ipCamList, tmpListItem ) == CAMLIST_ADD_DUP) {
				free(tmpListItem);
			} else {
				debug("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", aPkt.mac[0], aPkt.mac[1], aPkt.mac[2], aPkt.mac[3], aPkt.mac[4], aPkt.mac[5]);
				debug("Product #%07u : Unit #%04u\n", ntohl(aPkt.product_id), ntohl(aPkt.ser_no));
				debug("%s\n", tmpListItem->hwinfo);
				newCamCount++;
			}
		}
	} while(wait_us > 0);

	close(s);
	return newCamCount;
}


/**
 * Configures the IP address of one specific camera.
 *
 * @param camInfo		Structure describing the camera to configure
 * @param ipAddress	An ASCII string containing the new IP address to assign (e.g., "192.168.0.5")
 * @param wait_us		The number of microseconds to wait for a reply from the camera
 *
 * @return 	Returns -1 with errno set for system call failures.
 * 			Returns 0 for success
 * 			Returns ERR_TIMEOUT if no reply is received before the timeout expires
 * 			Returns ERR_CONFIG_ARPFAIL if the camera was configured successfully but
 * 					the host system could not update its arp table. This is normally because
 * 					the library is not running as root.
 */
int pr2Configure( IpCamList *camInfo, const char *ipAddress, unsigned wait_us) {
	// Create and initialize a new Configure packet
	PacketConfigure cPkt;

	cPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	cPkt.hdr.type = htonl(PKTT_CONFIGURE);
	cPkt.product_id = htonl(CONFIG_PRODUCT_ID);
	strncpy(cPkt.hdr.hrt, "CONFIGURE", sizeof(cPkt.hdr.hrt));

	cPkt.ser_no = htonl(camInfo->serial);

	struct in_addr newIP;
	inet_aton(ipAddress, &newIP);
	cPkt.ip_addr = newIP.s_addr;


	// Create and send the Configure broadcast packet. It is sent broadcast
	// because the camera does not yet have an IP address assigned.

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &cPkt.hdr.reply_to);
	if(s == -1) {
		perror("pr2Configure socket creation failed");
		return -1;
	}

	if(wgSendUDPBcast(s, camInfo->ifName, &cPkt, sizeof(cPkt)) == -1) {
	  debug("Unable to send broadcast\n");
    close(s);
		return -1;
	}


	// 'Connect' insures we will only receive datagram replies from the camera's new IP
	IPAddress camIP = newIP.s_addr;
	if( wgSocketConnect(s, &camIP) ) {
    close(s);
		return -1;
	}

	// Wait up to wait_us for a valid packet to be received on s
	do {
		if( !wgWaitForPacket(s, PKTT_ANNOUNCE, sizeof(PacketAnnounce), &wait_us)  && (wait_us != 0) ) {
			PacketAnnounce aPkt;

			if( recvfrom( s, &aPkt, sizeof(PacketAnnounce), 0, NULL, NULL )  == -1 ) {
				perror("wgDiscover unable to receive from socket");
        close(s);
				return -1;
			}

			// Need to have a valid packet from a camera with the expected serial number
			if( ntohl(aPkt.ser_no) == camInfo->serial ) {
				camInfo->status = CamStatusConfigured;
				memcpy(&camInfo->ip, &cPkt.ip_addr, sizeof(IPAddress));
				// Add the IP/MAC mapping to the system ARP table
				if( wgArpAdd(camInfo) ) {
					close(s);
					return ERR_CONFIG_ARPFAIL;
				}
				break;
			}
		}
	} while(wait_us > 0);
	close(s);

	if(wait_us != 0) {
		return 0;
	} else {
		return ERR_TIMEOUT;
	}
}

/**
 * Instructs one camera to begin streaming video to the host/port specified.
 *
 * @param camInfo Structure that describes the camera to contact
 * @param mac		Contains the MAC address of the host that will receive the video
 * @param ipAddress An ASCII string in dotted quad form containing the IP address of the host
 * 						that will receive the video (e.g., "192.168.0.5")
 * @param port		The port number that the video should be sent to. Host byte order.
 *
 * @return 	Returns -1 with errno set for system call failures
 * 			Returns 0 for success
 * 			Returns 1 for protocol failures (timeout, etc)
 */
int pr2StartVid( const IpCamList *camInfo, const uint8_t mac[6], const char *ipAddress, uint16_t port ) {
	PacketVidStart vPkt;

	// Initialize the packet
	vPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	vPkt.hdr.type = htonl(PKTT_VIDSTART);
	strncpy(vPkt.hdr.hrt, "Start Video", sizeof(vPkt.hdr.hrt));


	// Convert the ipAddress string into a binary value in network byte order
	inet_aton(ipAddress, (struct in_addr*)&vPkt.receiver.addr);

	// Convert the receiver port into network byte order
	vPkt.receiver.port = htons(port);

	// Copy the MAC address into the vPkt
	memcpy(&vPkt.receiver.mac, mac, 6);

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &vPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(wgSendUDP(s, &camInfo->ip, &vPkt, sizeof(vPkt)) == -1) {
    goto err_out;
	}

	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) ) {
    goto err_out;
	}

	// Wait for a status reply
	uint32_t type, code;
	if( pr2StatusWait( s, STD_REPLY_TIMEOUT, &type, &code ) == -1) {
    goto err_out;
  }

	close(s);
	if(type == PKT_STATUST_OK) {
		return 0;
	} else {
		debug("Error: wgStatusWait returned status %d, code %d\n", type, code);
		return 1;
	}

err_out:
  close(s);
  return -1;
}

/**
 * Instructs one camera to stop sending video.
 *
 * @param  camInfo Describes the camera to send the stop to.
 * @return 	Returns 0 for success
 * 			Returns -1 with errno set for system errors
 * 			Returns 1 for protocol errors
 */
int pr2StopVid( const IpCamList *camInfo ) {
	PacketVidStop vPkt;

	// Initialize the packet
	vPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	vPkt.hdr.type = htonl(PKTT_VIDSTOP);
	strncpy(vPkt.hdr.hrt, "Stop Video", sizeof(vPkt.hdr.hrt));

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &vPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &vPkt, sizeof(vPkt)) == -1 ) {
    goto err_out;
	}

	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) == -1) {
    goto err_out;
	}

	uint32_t type, code;
	if(pr2StatusWait( s, STD_REPLY_TIMEOUT, &type, &code ) == -1) {
    goto err_out;
	}

	close(s);
	if(type == PKT_STATUST_OK) {
		return 0;
	} else {
		debug("Error: pr2StatusWait returned status %d, code %d\n", type, code);
		return 1;
	}

err_out:
  close(s);
  return -1;
}

/**
 * Instructs one camera to execute a soft reset.
 *
 * @param camInfo Describes the camera to reset.
 * @return 	Returns 0 for success
 * 			Returns -1 for system errors
 */
int pr2Reset( IpCamList *camInfo ) {
	PacketReset gPkt;

	// Initialize the packet
	gPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	gPkt.hdr.type = htonl(PKTT_RESET);
	strncpy(gPkt.hdr.hrt, "Reset", sizeof(gPkt.hdr.hrt));

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &gPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &gPkt, sizeof(gPkt)) == -1 ) {
	  close(s);
    return -1;
	}


	close(s);

	// Camera is no longer configured after a reset
	camInfo->status = CamStatusDiscovered;

	// There is no reply to a reset packet

	return 0;
}

/**
 * Gets the system time of a specified camera.
 *
 * In the camera, system time is tracked as a number of clock 'ticks' since
 * the last hard reset. This function receives the number of 'ticks' as well as
 * a 'ticks_per_sec' conversion factor to return a 64-bit time result in microseconds.
 *
 * @param camInfo Describes the camera to connect to
 * @param time_us Points to the location to store the system time in us
 *
 * @return Returns 0 for success, -1 for system error, or 1 for protocol error.
 */
int pr2GetTimer( const IpCamList *camInfo, uint64_t *time_us ) {
	PacketTimeRequest gPkt;

	// Initialize the packet
	gPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	gPkt.hdr.type = htonl(PKTT_TIMEREQ);
	strncpy(gPkt.hdr.hrt, "Time Req", sizeof(gPkt.hdr.hrt));

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &gPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &gPkt, sizeof(gPkt)) == -1 ) {
    close(s);
		return -1;
	}

	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) ) {
    close(s);
		return -1;
	}

	uint32_t wait_us = STD_REPLY_TIMEOUT;
	do {
		if( !wgWaitForPacket(s, PKTT_TIMEREPLY, sizeof(PacketTimer), &wait_us)  && (wait_us != 0) ) {
			PacketTimer tPkt;
			if( recvfrom( s, &tPkt, sizeof(PacketTimer), 0, NULL, NULL )  == -1 ) {
				perror("GetTime unable to receive from socket");
        close(s);
				return -1;
			}

			*time_us = (uint64_t)ntohl(tPkt.ticks_hi) << 32;
			*time_us += ntohl(tPkt.ticks_lo);

			// Need to multiply final result by 1E6 to get us from sec
			// Try to do this to limit loss of precision without overflow
			// (We will overflow the 64-bit type with this approach
			// after the camera has been up for about 11 continuous years)
			//FIXME: Review this algorithm for possible improvements.
			*time_us *= 1000;
			*time_us /= (ntohl(tPkt.ticks_per_sec)/1000);
			//debug("Got time of %llu us since last reset\n", *time_us);
			close(s);
			return 0;
		}
	} while(wait_us > 0);

	debug("Timed out waiting for time value\n");
	close(s);
	return 1;
}

/**
 * Reads one FLASH_PAGE_SIZE byte page of the camera's onboard Atmel dataflash.
 *
 * @param camInfo Describes the camera to connect to.
 * @param address Specifies the 12-bit flash page address to read (0-4095)
 * @param pageDataOut Points to at least FLASH_PAGE_SIZE bytes of storage in which to place the flash data.
 *
 * @return 	Returns 0 for success
 * 			Returns -1 for system error
 * 			Returns 1 for protocol error
 *
 */

int pr2FlashRead( const IpCamList *camInfo, uint32_t address, uint8_t *pageDataOut ) {
	PacketFlashRequest rPkt;

	// Initialize the packet
	rPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	rPkt.hdr.type = htonl(PKTT_FLASHREAD);
	if(address > FLASH_MAX_PAGENO) {
		return 1;
	}

	// The page portion of the address is 12-bits wide, range (bit9..bit21)
	rPkt.address = htonl(address<<9);

	strncpy(rPkt.hdr.hrt, "Flash read", sizeof(rPkt.hdr.hrt));

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &rPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &rPkt, sizeof(rPkt)) == -1 ) {
    close(s);
		return -1;
	}


	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) ) {
    close(s);
		return -1;
	}

	uint32_t wait_us = STD_REPLY_TIMEOUT;
	do {
		if( !wgWaitForPacket(s, PKTT_FLASHDATA, sizeof(PacketFlashPayload), &wait_us)  && (wait_us != 0) ) {
			PacketFlashPayload fPkt;
			if( recvfrom( s, &fPkt, sizeof(PacketFlashPayload), 0, NULL, NULL )  == -1 ) {
				perror("GetTime unable to receive from socket");
        close(s);
				return -1;
			}

			// Copy the received data into the space pointed to by pageDataOut
			memcpy(pageDataOut, fPkt.data, FLASH_PAGE_SIZE);
			close(s);
			return 0;
		}
	} while(wait_us > 0);

	debug("Timed out waiting for flash value\n");
	close(s);
	return 1;
}

/**
 * Writes one FLASH_PAGE_SIZE byte page to the camera's onboard Atmel dataflash.
 *
 * @param camInfo Describes the camera to connect to.
 * @param address Specifies the 12-bit flash page address to write (0-4095)
 * @param pageDataOut Points to at least FLASH_PAGE_SIZE bytes of storage from which to get the flash data.
 *
 * @return 	Returns 0 for success
 * 			Returns -1 for system error
 * 			Returns 1 for protocol error
 *
 */
int pr2FlashWrite( const IpCamList *camInfo, uint32_t address, const uint8_t *pageDataIn ) {
	PacketFlashPayload rPkt;

	// Initialize the packet
	rPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	rPkt.hdr.type = htonl(PKTT_FLASHWRITE);
	if(address > FLASH_MAX_PAGENO) {
		return 1;
	}

	// The page portion of the address is 12-bits wide, range (bit9..bit21)
	rPkt.address = htonl(address<<9);
	strncpy(rPkt.hdr.hrt, "Flash write", sizeof(rPkt.hdr.hrt));

	memcpy(rPkt.data, pageDataIn, FLASH_PAGE_SIZE);

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &rPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &rPkt, sizeof(rPkt)) == -1 ) {
    close(s);
		return -1;
	}

	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) ) {
    close(s);
		return -1;
	}

	// Wait for response - once we get an OK, we're clear to send the next page.
	uint32_t type, code;
	pr2StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

	close(s);
	if(type == PKT_STATUST_OK) {
		return 0;
	} else {
		debug("Error: wgStatusWait returned status %d, code %d\n", type, code);
		return 1;
	}
}

/**
 * Sets the trigger type (internal or external) for one camera.
 *
 * @param camInfo Describes the camera to connect to
 * @param triggerType Can be either TRIG_STATE_INTERNAL or TRIG_STATE_EXTERNAL
 *
 * @return 	Returns 0 for success
 * 			Returns -1 for system error
 * 			Returns 1 for protocol error
 */
int pr2TriggerControl( const IpCamList *camInfo, uint32_t triggerType ) {
	PacketTrigControl tPkt;

	// Initialize the packet
	tPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	tPkt.hdr.type = htonl(PKTT_TRIGCTRL);
	tPkt.trig_state = htonl(triggerType);

	if(triggerType == TRIG_STATE_INTERNAL ) {
		strncpy(tPkt.hdr.hrt, "Int. Trigger", sizeof(tPkt.hdr.hrt));
	} else {
		strncpy(tPkt.hdr.hrt, "Ext. Trigger", sizeof(tPkt.hdr.hrt));
	}

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &tPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &tPkt, sizeof(tPkt)) == -1 ) {
    close(s);
		return -1;
	}

	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) ) {
    close(s);
		return -1;
	}

	// Wait for response
	uint32_t type, code;
	pr2StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

	close(s);
	if(type == PKT_STATUST_OK) {
		return 0;
	} else {
		debug("Error: wgStatusWait returned status %d, code %d\n", type, code);
		return 1;
	}
}

/**
 * Sets the permanent serial number and MAC configuration for one camera
 *
 * @warning: This command can only be sent once per camera. The changes are permanent.
 *
 * @param camInfo Describes the camera to connect to
 * @param serial Is the 32-bit unique serial number to assign (the product ID portion is already fixed)
 * @param mac Is the 48-bit unique MAC address to assign to the board
 *
 * @return 	Returns 0 for success
 * 			Returns -1 for system error
 * 			Returns 1 for protocol error
 */
int pr2ConfigureBoard( const IpCamList *camInfo, uint32_t serial, MACAddress *mac ) {
	PacketSysConfig sPkt;

	// Initialize the packet
	sPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	sPkt.hdr.type = htonl(PKTT_SYSCONFIG);

	strncpy(sPkt.hdr.hrt, "System Config", sizeof(sPkt.hdr.hrt));
	memcpy(&sPkt.mac, mac, 6);
	sPkt.serial = htonl(serial);


	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &sPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &sPkt, sizeof(sPkt)) == -1 ) {
    close(s);
		return -1;
	}

	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) ) {
    close(s);
		return -1;
	}
	// Wait for response
	uint32_t type, code;
	pr2StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

	close(s);
	if(type == PKT_STATUST_OK) {
		return 0;
	} else {
		debug("Error: wgStatusWait returned status %d, code %d\n", type, code);
		return 1;
	}
}

/**
 * Writes to one image sensor I2C register on one camera.
 *
 * @param camInfo Describes the camera to connect to
 * @param reg The 8-bit register address to write into
 * @parm data 16-bit value to write into the register
 *
 * @return 	Returns 0 for success
 * 			Returns -1 for system error
 * 			Returns 1 for protocol error
 */
int pr2SensorWrite( const IpCamList *camInfo, uint8_t reg, uint16_t data ) {
	PacketSensorData sPkt;

	// Initialize the packet
	sPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	sPkt.hdr.type = htonl(PKTT_SENSORWR);

	strncpy(sPkt.hdr.hrt, "Write I2C", sizeof(sPkt.hdr.hrt));
	sPkt.address = reg;
	sPkt.data = htons(data);

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &sPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &sPkt, sizeof(sPkt)) == -1 ) {
    close(s);
		return -1;
	}

	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) ) {
    close(s);
		return -1;
	}

	// Wait for response
	uint32_t type, code;
	pr2StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

	close(s);
	if(type == PKT_STATUST_OK) {
		return 0;
	} else {
		debug("Error: wgStatusWait returned status %d, code %d\n", type, code);
		return 1;
	}
}

/**
 * Reads the value of one image sensor I2C register on one camera.
 *
 * @param camInfo Describes the camera to connect to
 * @param reg The 8-bit register address to read from
 * @parm data Pointer to 16 bits of storage to write the value to
 *
 * @return 	Returns 0 for success
 * 			Returns -1 for system error
 * 			Returns 1 for protocol error
 */
int pr2SensorRead( const IpCamList *camInfo, uint8_t reg, uint16_t *data ) {
	PacketSensorRequest rPkt;

	// Initialize the packet
	rPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	rPkt.hdr.type = htonl(PKTT_SENSORRD);
	rPkt.address = reg;
	strncpy(rPkt.hdr.hrt, "Read I2C", sizeof(rPkt.hdr.hrt));

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &rPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &rPkt, sizeof(rPkt)) == -1 ) {
    close(s);
		return -1;
	}

	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) ) {
    close(s);
		return -1;
	}

	uint32_t wait_us = STD_REPLY_TIMEOUT;
	do {
		if( !wgWaitForPacket(s, PKTT_SENSORDATA, sizeof(PacketSensorData), &wait_us)  && (wait_us != 0) ) {
			PacketSensorData sPkt;
			if( recvfrom( s, &sPkt, sizeof(PacketSensorData), 0, NULL, NULL )  == -1 ) {
				perror("SensorRead unable to receive from socket");
        close(s);
				return -1;
			}

			*data = ntohs(sPkt.data);
			close(s);
			return 0;
		}
	} while(wait_us > 0);

	debug("Timed out waiting for sensor value\n");
	close(s);
	return 1;
}

/**
 * Sets the address of one of the four automatically read I2C registers on one camera.
 *
 * @param camInfo Describes the camera to connect to
 * @param index The index (0..3) of the register to set
 * @param reg The 8-bit address of the register to read at position 'index', or I2C_AUTO_REG_UNUSED.
 *
 * @return 	Returns 0 for success
 * 			Returns -1 for system error
 * 			Returns 1 for protocol error
 */
int pr2SensorSelect( const IpCamList *camInfo, uint8_t index, uint32_t reg ) {
	PacketSensorSelect sPkt;

	// Initialize the packet
	sPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	sPkt.hdr.type = htonl(PKTT_SENSORSEL);

	strncpy(sPkt.hdr.hrt, "Select I2C", sizeof(sPkt.hdr.hrt));
	sPkt.index = index;
	sPkt.address = htonl(reg);

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &sPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &sPkt, sizeof(sPkt)) == -1 ) {
    close(s);
		return -1;
	}

	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) ) {
    close(s);
		return -1;
	}

	// Wait for response
	uint32_t type, code;
	pr2StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

	close(s);
	if(type == PKT_STATUST_OK) {
		return 0;
	} else {
		debug("Error: wgStatusWait returned status %d, code %d\n", type, code);
		return 1;
	}
}

/**
 * Selects one of the 10 pre-programmed imager modes in the specified camera.
 *
 * @param camInfo Describes the camera to connect to
 * @param mode The mode number to select, range (0..9)
 *
 * @return 	Returns 0 for success
 * 			Returns -1 for system error
 * 			Returns 1 for protocol error
 */
int pr2ImagerModeSelect( const IpCamList *camInfo, uint32_t mode ) {
	PacketImagerMode mPkt;

	// Initialize the packet
	mPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	mPkt.hdr.type = htonl(PKTT_IMGRMODE);

	mPkt.mode = htonl(mode);

	strncpy(mPkt.hdr.hrt, "Set Mode", sizeof(mPkt.hdr.hrt));

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &mPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &mPkt, sizeof(mPkt)) == -1 ) {
    close(s);
		return -1;
	}

	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) ) {
    close(s);
		return -1;
	}

	// Wait for response
	uint32_t type, code;
	pr2StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

	close(s);
	if(type == PKT_STATUST_OK) {
		return 0;
	} else {
		debug("Error: wgStatusWait returned status %d, code %d\n", type, code);
		return 1;
	}
}

/**
 * Provides for manually setting non-standard imager modes. Use only with extreme caution.
 *
 * @warning The imager frame size set by I2C must match the one used by the firmware, or system
 * failure could result.
 *
 * @param camInfo Describes the camera to connect to
 * @param horizontal The number of 8-bit image pixels per line of video. Maximum value 752.
 * @param vertical The number of video lines per frame.
 *
 * @return 	Returns 0 for success
 * 			Returns -1 for system error
 * 			Returns 1 for protocol error
 */
int pr2ImagerSetRes( const IpCamList *camInfo, uint16_t horizontal, uint16_t vertical ) {
	PacketImagerSetRes rPkt;

	// Initialize the packet
	rPkt.hdr.magic_no = htonl(WG_MAGIC_NO);
	rPkt.hdr.type = htonl(PKTT_IMGRSETRES);

	rPkt.horizontal = htons(horizontal);
	rPkt.vertical = htons(vertical);

	strncpy(rPkt.hdr.hrt, "Set Res", sizeof(rPkt.hdr.hrt));

	/* Create a new socket bound to a local ephemeral port, and get our local connection
	 * info so we can request a reply back to the same socket.
	 */
	int s = wgCmdSocketCreate(camInfo->ifName, &rPkt.hdr.reply_to);
	if( s == -1 ) {
		return -1;
	}

	if(	wgSendUDP(s, &camInfo->ip, &rPkt, sizeof(rPkt)) == -1 ) {
    close(s);
		return -1;
	}

	// 'Connect' insures we will only receive datagram replies from the camera we've specified
	if( wgSocketConnect(s, &camInfo->ip) ) {
    close(s);
		return -1;
	}

	// Wait for response
	uint32_t type, code;
	pr2StatusWait( s, STD_REPLY_TIMEOUT, &type, &code );

	close(s);
	if(type == PKT_STATUST_OK) {
		return 0;
	} else {
		debug("Error: wgStatusWait returned status %d, code %d\n", type, code);
		return 1;
	}
}


#define MAX_HORIZ_RESOLUTION 752
#define LINE_NUMBER_MASK 0x3FF


int pr2VidReceive( const char *ifName, uint16_t port, size_t height, size_t width, FrameHandler frameHandler, void *userData ) {
	struct in_addr host_addr;
	wgIpGetLocalAddr( ifName, &host_addr );

	if( frameHandler == NULL ) {
		debug("Invalid frame handler, aborting.\n");
		return 1;
	}

	debug("pr2VidReceive ready to receive on %s (%s:%u)\n", ifName, inet_ntoa(host_addr), port);

	int s = wgSocketCreate( &host_addr, port );
	if( s == -1 ) {
		return -1;
	}

	/*
	 * The default receive buffer size on a 32-bit Linux machine is only 128kB.
	 * At a burst data rate of ~ 82.6Mbit/s in the 640x480 @30fps, this buffer will fill in ~12.6ms.
	 * With the default Linux scheduler settings, this leaves virtually no time for any other processes
	 * to execute before the buffer overflows. Increasing the buffer reduces the chance of lost
	 * packets.
	 *
	 * The 8MB buffer here is far larger than necessary for most applications and may be reduced with
	 * some experimentation.
	 *
	 * Note that the Linux system command 'sysctl -w net.core.rmem_max=8388608' must be used to enable
	 * receive buffers larger than the kernel default.
	 */
	size_t bufsize = 8*1024*1024;	// 8MB receive buffer.

	if( setsockopt(s, SOL_SOCKET,SO_RCVBUF, &bufsize, sizeof(bufsize)) == -1) {
		perror("Can't set rcvbuf option");
    close(s);
		return -1;
	}

  socklen_t bufsizesize = sizeof(bufsize);
	if( getsockopt(s, SOL_SOCKET,SO_RCVBUF, &bufsize, &bufsizesize) == 0) {
		debug("Receive buffer size is: %i (%i)\n", bufsize, bufsizesize);
	}
  else
  {
		perror("Can't read receive buffer size");
  }

	// We receive data one line at a time; lines will be reassembled into this buffer as they arrive
	uint8_t *frame_buf;
	frame_buf = malloc(sizeof(uint8_t)*width*height);
	if(frame_buf == NULL) {
		perror("Can't malloc frame buffer");
    close(s);
		return -1;
	}

	// Allocate enough storage for the header as well as 'width' bytes of video data
	PacketVideoLine *vPkt=malloc(sizeof(PacketVideoLine));
	if(vPkt == NULL) {
		perror("Can't malloc line packet buffer");
    close(s);
		return -1;
	}

	// Flag to indicate that 'frameInfo.frame_number' has not yet been set
	bool firstPacket = true;

	// Flag to indicate the current frame is complete (either EOF received or first line of next frame received)
	bool frameComplete;

  // Flag to indicate that we have set the 'frameInfo.startTime' timeval for this frame.
  bool frameStartTimeSet;

	// Holds return code from frameHandler
	int handlerReturn;

	// Counts number of lines received in current frame
	uint32_t lineCount;

	// Points to an EOF structure for passing to frameHandler;
	PacketEOF *eof = NULL;

  // Information structure to pass to the frame handler.
  pr2FrameInfo frameInfo;
  
  frameInfo.width = width;
  frameInfo.height = height;

	do {
    lineCount = 0;
		frameComplete = false;
    frameStartTimeSet = false;
		frameInfo.lastMissingLine = -1;
    frameInfo.missingLines = 0;
    frameInfo.shortFrame = false;
    frameInfo.frame_number = vPkt->header.frame_number+1;
    // Ensure the buffer is cleared out. Makes viewing short packets less confusing; missing lines will be black.
		memset(frame_buf, 0, width*height);

		// We detect a dropped EOF by unexpectedly receiving a line from the next frame before getting the EOF from this frame.
		// After the last frame has been processed, start a fresh frame with the expected line.
		if( (eof==NULL) && (firstPacket==false) ) {
			if(vPkt->header.line_number < height ) {
				memcpy(&(frame_buf[vPkt->header.line_number*width]), vPkt->data, width);
				lineCount++;
			}
			frameInfo.frame_number &= ~0xFFFF;
			frameInfo.frame_number |= vPkt->header.frame_number;
		}

		do {
			// Wait forever for video packets to arrive; could use select() with a timeout here if a timeout is needed
      struct sockaddr_in fromaddr;
      struct timeval readtimeout;
      fd_set set;
      socklen_t fromaddrlen = sizeof(struct sockaddr_in);
      fromaddr.sin_family = AF_INET;

      // Wait for either a packet to be received or for timeout
      handlerReturn = 0;
      do {
        readtimeout.tv_sec = 1;
        readtimeout.tv_usec = 0;
      
        FD_ZERO(&set);
        FD_SET(s, &set);
        
        if( select(s+1, &set, NULL, NULL, &readtimeout) == -1 ) {
          perror("pr2VidReceive select failed");
          close(s);
          return -1;
        }

        // Call the frame handler with NULL to see if we should bail out. 
        if(! FD_ISSET(s, &set) && errno != EINTR) {
          debug("Select timed out. Calling handler.");
          handlerReturn = frameHandler(NULL, userData);
          if (handlerReturn)
            break;
        }
      } while (! FD_ISSET(s, &set));
      
      // We timed out, and the handler returned nonzero.
      if (handlerReturn) 
        break;
      
      if( recvfrom( s, vPkt, sizeof(HeaderVideoLine)+width, 0, (struct sockaddr *) &fromaddr, &fromaddrlen )  == -1 ) {
				perror("pr2VidReceive unable to receive from socket");
				break;
			}
				
			// Convert fields to host byte order for easier processing
			vPkt->header.frame_number = ntohl(vPkt->header.frame_number);
			vPkt->header.line_number = ntohs(vPkt->header.line_number);
			vPkt->header.horiz_resolution = ntohs(vPkt->header.horiz_resolution);
			vPkt->header.vert_resolution = ntohs(vPkt->header.vert_resolution);

			//debug("frame: #%i line: %i\n", vPkt->header.frame_number, 0x3FF & vPkt->header.line_number);

			// Check to make sure the frame number information is consistent within the packet
			uint16_t temp = (vPkt->header.line_number>>10) & 0x003F;
			if (((vPkt->header.line_number & IMAGER_MAGICLINE_MASK) == 0) && (temp != (vPkt->header.frame_number & 0x003F))) {
				debug("Mismatched line/frame numbers: %02X / %02X\n", temp, (vPkt->header.frame_number & 0x003F));
			}

			// Validate that the frame is the size we expected
			if( (vPkt->header.horiz_resolution != width) || (vPkt->header.vert_resolution != height) ) {
				debug("Invalid frame size received: %u x %u, expected %u x %u\n", vPkt->header.horiz_resolution, vPkt->header.vert_resolution, width, height);
        close(s);
				return 1;
			}

			// First time through we need to initialize the number of the first frame we received
			if(firstPacket == true) {
				firstPacket = false;
				frameInfo.frame_number = vPkt->header.frame_number;
			}
	
      // Store the start time for the frame.
			if (!frameStartTimeSet)
      {
        gettimeofday(&frameInfo.startTime, NULL);
        frameStartTimeSet = true;
      }

			// Check for frames that ended with an error
			if( (vPkt->header.line_number == IMAGER_LINENO_ERR) || 
			    (vPkt->header.line_number == IMAGER_LINENO_OVF) ) {
				debug("Video error: %04X\n", vPkt->header.line_number);

				// In the case of these special 'error' EOF packets, there has been a serious internal camera failure
				// so we will abort rather than try to process the last frame.
				break;
			} else if (vPkt->header.line_number == IMAGER_LINENO_ABORT) {
				debug("Video aborted\n");
				break;  //don't process last frame
			} else if((vPkt->header.frame_number - frameInfo.frame_number) & 0xFFFF) { // The camera only sends 16 bits of frame number
				// If we have received a line from the next frame, we must have missed the EOF somehow
				debug ("Frame #%u missing EOF, got %i lines\n", frameInfo.frame_number, lineCount);
				frameComplete = true;
				// EOF was missing
				eof = NULL;
			} else if( vPkt->header.line_number == IMAGER_LINENO_EOF ) {
				// This is a 'normal' (non-error) end of frame packet (line number 0x3FF)
				frameComplete = true;

				// Handle EOF data fields
				eof = (PacketEOF *)vPkt;

				// Correct to network byte order for frameHandler
				eof->ticks_hi = ntohl(eof->ticks_hi);
				eof->ticks_lo = ntohl(eof->ticks_lo);
				eof->ticks_per_sec = ntohl(eof->ticks_per_sec);

				// Correct to network byte order for frameHandler
				eof->i2c_valid = ntohl(eof->i2c_valid);
				for(int i=0; i<I2C_REGS_PER_FRAME; i++) {
					eof->i2c[i] = ntohs(eof->i2c[i]);
				}

				if(lineCount != height) {
					// Flag packet as being short for the frameHandler
					eof->header.line_number = IMAGER_LINENO_SHORT;
          frameInfo.shortFrame = true;
				}
				// Move to the next frame
			
      } else {
				// Remove extraneous frame information from the line number field
				vPkt->header.line_number &= LINE_NUMBER_MASK;

				if( vPkt->header.line_number >= vPkt->header.vert_resolution ) {
					debug("Invalid line number received: %u (max %u)\n", vPkt->header.line_number, vPkt->header.vert_resolution);
					break;
				}
			  if (lineCount + frameInfo.missingLines < vPkt->header.line_number)
        {
          int missedLines = vPkt->header.line_number - lineCount - frameInfo.missingLines; 
          frameInfo.lastMissingLine = vPkt->header.line_number - 1;
          debug("Frame #%i missed %i line(s) starting at %i src port %i\n", vPkt->header.frame_number, 
              missedLines, lineCount + frameInfo.missingLines, ntohs(fromaddr.sin_port));
          frameInfo.missingLines += missedLines;
        }
				memcpy(&(frame_buf[vPkt->header.line_number*width]), vPkt->data, width);
				lineCount++;
      }
		} while(frameComplete == false);

		if( frameComplete == true ) {
      frameInfo.frameData = frame_buf;
      frameInfo.eofInfo = eof;
      frameInfo.frame_number = frameInfo.frame_number; 
			handlerReturn = frameHandler(&frameInfo, userData);
		} else {
			// We wind up here if a serious error has resulted in us 'break'ing out of the loop.
			// We won't call frameHandler, we'll just exit directly.
			handlerReturn = -1;
		}
	} while( handlerReturn == 0 );

	close(s);
	return 0;
}
