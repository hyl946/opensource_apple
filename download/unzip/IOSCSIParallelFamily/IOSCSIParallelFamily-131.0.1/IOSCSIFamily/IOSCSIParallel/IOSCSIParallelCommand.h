/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *
 *	IOSCSICommand.h
 *
 */
#ifndef _IOSCSIPARALLELCOMMAND_H
#define _IOSCSIPARALLELCOMMAND_H

#include "IOSCSICommand.h"
#include "SCSIParallelTarget.h"

class IOSCSIParallelDevice;
class IOSCSIParallelCommand;
class IOSyncer;

class IOSCSIParallelCommand : public IOSCSICommand
{
    OSDeclareDefaultStructors(IOSCSIParallelCommand)

    friend class IOSCSIParallelController;
    friend class IOSCSIParallelDevice;

/*------------------Methods provided to IOCDBCommand users -------------------------*/
public:  
    /*
     * Set/Get IOMemoryDescriptor object to I/O data buffer or sense data buffer.
     */
     void 			setPointers( IOMemoryDescriptor 	*desc, 
					     UInt32 			transferCount, 
                                             bool 			isWrite, 
					     bool 			isSense = false );

     void 			getPointers( IOMemoryDescriptor 	**desc, 
					     UInt32 			*transferCount, 
					     bool 			*isWrite, 
					     bool 			isSense = false );
    /*
     * Set/Get command timeout (mS)
     */	 	
     void 			setTimeout( UInt32  timeoutmS );		
     UInt32 			getTimeout();

    /*
     * Set async callback routine. Specifying no parameters indicates synchronous call.
     */
     void 			setCallback( void *target = 0, CallbackFn callback = 0, void *refcon = 0 );

    /*
     * Set/Get CDB information. (Generic CDB version)
     */	
     void 			setCDB( CDBInfo *cdbInfo );
     void 			getCDB( CDBInfo *cdbInfo );

    /*
     * Get CDB results. (Generic CDB version)
     */      
     IOReturn			getResults( CDBResults *cdbResults );

    /*
     * Get CDB Device this command is directed to.
     */
     IOCDBDevice 		*getDevice( IOCDBDevice *deviceType );

    /*
     * Command verbs
     */ 
     bool 			execute( UInt32 *sequenceNumber = 0 );
     void			abort( UInt32 sequenceNumber );
     void	 		complete();

    /*
     * Get pointers to client and command data.
     */
     void			*getCommandData();
     void			*getClientData();

    /*
     * Get unique sequence number assigned to command.
     */
    UInt32			getSequenceNumber();

/*------------------ Additional methods provided to IOSCSICommand users -------------------------*/
public:
    /*
     * Set/Get CDB information. (SCSI specific version).
     */	
     void 			setCDB( SCSICDBInfo *scsiCmd );
     void			getCDB( SCSICDBInfo *scsiCmd );

    /*
     * Get/Set CDB results. (SCSI specific version).
     */      
     IOReturn			getResults( SCSIResults *results );
     void			setResults( SCSIResults *results, SCSINegotiationResults *negotiationResults );

    /*
     * Get SCSI Device this command is directed to.
     */
     IOSCSIParallelDevice 	*getDevice( IOSCSIParallelDevice *deviceType );


    /*
     * Get SCSI Target/Lun for this command.
     */
     void			getTargetLun( SCSITargetLun *targetLun );

    /*
     * Get/Set queue routing for this command.
     */
    void			setQueueInfo(  UInt32 forQueueType = kQTypeNormalQ, UInt32 forQueuePosition = kQPositionTail );
    void			getQueueInfo(  UInt32 *forQueueType, UInt32 *forQueuePosition = 0 );

    /*
     * Get command type / Get original command. 
     *
     * These methods are provided for the controller class to identify and relate commands.
     * They are not usually of interest to the client side.
     */     
     UInt32			getCmdType();
     IOSCSIParallelCommand	*getOriginalCmd();    

    /*
     * Set to blank state, call prior to re-use of this object.
     */	
     void 			zeroCommand();  

/*------------------Methods private to the IOSCSICommand class-------------------------*/
public:
    void 			free();

     IOSCSIDevice 		*getDevice( IOSCSIDevice *deviceType );
     void			setResults( SCSIResults *results );
     
private:
    IOReturn 			adapterStatusToIOReturnCode( SCSIAdapterStatus adapterStatus );
    IOReturn 			scsiStatusToIOReturnCode( UInt8 scsiStatus );

private:
    SCSICommandType		cmdType;

    IOSCSIParallelController    *controller;
    IOSCSIParallelDevice	*device;

    queue_head_t		*list;
    queue_chain_t		nextCommand;
 
    SCSICDBInfo			scsiCmd;
    SCSIResults			results;

    UInt32			timeout;    
    UInt32			timer;

    UInt8			queueType;
    UInt8			queuePosition;
    
    IOMemoryDescriptor		*xferDesc;
    UInt32			xferCount;
    UInt32			xferDirection;

    UInt32			senseLength;
    IOMemoryDescriptor		*senseData;

    IOSCSIParallelCommand	*origCommand;

    union
    {
        struct
        {
            UInt32		reserved;
            IOSyncer *		lock;
        } sync;
        struct
        {
	    CallbackFn 		callback;
    	    void		*target;
    	    void		*refcon;
        } async;
     } completionInfo;

    UInt32			dataSize;
    void                	*dataArea;
    void			*commandPrivateData;
    void			*clientData;	
    
    UInt32			sequenceNumber;	  
};

#endif
