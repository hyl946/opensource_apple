/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2001 Apple Computer, Inc.  All rights reserved. 
 *
 * HISTORY
 *
 * 18-Dec-01 ebold created
 *
 */
 
#include <CoreFoundation/CoreFoundation.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPreferencesPrivate.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecificPrivate.h>
#include <SystemConfiguration/SCDPlugin.h>

#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOMessage.h>

#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <syslog.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include "powermanagementServer.h" // mig generated

#include "PMSettings.h"
#include "PSLowPower.h"
#include "BatteryTimeRemaining.h"
#include "AutoWakeScheduler.h"
#include "RepeatingAutoWake.h"
#include "SetActive.h"
#include "PrivateLib.h"

#define kIOPMAppName        "Power Management configd plugin"
#define kIOPMPrefsPath        "com.apple.PowerManagement.xml"

#ifndef kIOUPSDeviceKey
// Also defined in ioupsd/IOUPSPrivate.h
#define kIOUPSDeviceKey                 "UPSDevice" 
#endif

// Global keys
static CFStringRef              gTZNotificationNameString   = NULL;
static CFStringRef              EnergyPrefsKey              = NULL;
static CFStringRef              AutoWakePrefsKey            = NULL;

static io_connect_t             _pm_ack_port                = 0;
static io_iterator_t            _ups_added_noteref          = 0;
static int                      _alreadyRunningIOUPSD       = 0;
static int                      gClientUID                  = -1;
static int                      gClientGID                  = -1;

CFMachPortRef                   serverMachPort              = NULL;


// defined by MiG
extern boolean_t powermanagement_server(mach_msg_header_t *, mach_msg_header_t *);

// external
__private_extern__ void cleanupAssertions(mach_port_t dead_port);

// foward declarations
static void initializeESPrefsDynamicStore(void);
static void initializePowerSourceChangeNotification(void);
static void initializeMIGServer(void);
static void initializeInterestNotifications(void);
static void initializeTimezoneChangeNotifications(void);
static void intializeDisplaySleepNotifications(void);

static void SleepWakeCallback(void *,io_service_t, natural_t, void *);
static void ESPrefsHaveChanged(SCDynamicStoreRef, CFArrayRef, void *);
static void _ioupsd_exited(pid_t, int, struct rusage *, void *);
static void UPSDeviceAdded(void *, io_iterator_t);
static void BatteryMatch(void *, io_iterator_t);
static void BatteryInterest(void *, io_service_t, natural_t, void *);
static void PMUInterest(void *, io_service_t, natural_t, void *);
extern void PowerSourcesHaveChanged(void *info);
static void broadcastGMTOffset(void);

static void timeZoneChangedCallBack(
                CFNotificationCenterRef center, 
                void *observer, 
                CFStringRef notificationName, 
                const void *object, 
                CFDictionaryRef userInfo);

static void displayPowerStateChange(
                void *ref, 
                io_service_t service, 
                natural_t messageType, 
                void *arg);

static boolean_t pm_mig_demux(
                mach_msg_header_t * request,
                mach_msg_header_t * reply);

static void mig_server_callback(
                CFMachPortRef port, 
                void *msg, 
                CFIndex size, 
                void *info);

kern_return_t _io_pm_set_active_profile(
                mach_port_t         server,
                vm_offset_t         profiles_ptr,
                mach_msg_type_number_t    profiles_len,
                int                 *result);


/* prime
 *
 * configd entry point
 */
void prime()
{    
    // Initialize battery averaging code
    BatteryTimeRemaining_prime();
    
    // Initialize PMSettings code
    PMSettings_prime();
    
    // Initialize PSLowPower code
    PSLowPower_prime();

    // Initialzie AutoWake code
    AutoWake_prime();
    RepeatingAutoWake_prime();
    
    // initialize Assertions code
    PMAssertions_prime();
        
    return;
}

/* load
 *
 * configd entry point
 */
void load(CFBundleRef bundle, Boolean bundleVerbose)
{
    IONotificationPortRef           notify;    
    io_object_t                     anIterator;

    // Install notification on Power Source changes
    initializePowerSourceChangeNotification();

    // Install notification when the preferences file changes on disk
    initializeESPrefsDynamicStore();

    // Install notification on ApplePMU&IOPMrootDomain general interest messages
    initializeInterestNotifications();

    // Initialize MIG
    initializeMIGServer();
    
    // Initialize timezone changed notifications
    initializeTimezoneChangeNotifications();
    
    // Register for display dim/undim notifications
    intializeDisplaySleepNotifications();
    
    // Register for SystemPower notifications
    _pm_ack_port = IORegisterForSystemPower (0, &notify, SleepWakeCallback, &anIterator);
    if ( _pm_ack_port != MACH_PORT_NULL ) {
        if(notify) CFRunLoopAddSource(CFRunLoopGetCurrent(),
                            IONotificationPortGetRunLoopSource(notify),
                            kCFRunLoopDefaultMode);
    }

    return;
}



/* PMUInterest
 *
 * Receives and distributes messages from the PMU driver
 * These include legacy AutoWake requests and battery change notifications.
 */
static void 
PMUInterest(void *refcon, io_service_t service, natural_t messageType, void *arg)
{    
    // Tell the AutoWake handler
    if((kIOPMUMessageLegacyAutoWake == messageType) ||
       (kIOPMUMessageLegacyAutoPower == messageType) )
        AutoWakePMUInterestNotification(messageType, (UInt32)arg);
}


static void BatteryMatch(
    void *refcon, 
    io_iterator_t b_iter) 
{
    IOReturn                    ret;
    IOPMBattery                 *tracking;
    IONotificationPortRef       notify = (IONotificationPortRef)refcon;
    io_registry_entry_t         battery;
    io_object_t                 notification_ref;
    
    while(battery = (io_registry_entry_t)IOIteratorNext(b_iter)) 
    {
        // Add battery to our list of batteries
        tracking = _newBatteryFound(battery);
        
        // And install an interest notification on it
        ret = IOServiceAddInterestNotification(notify, battery, 
                            kIOGeneralInterest, BatteryInterest,
                            (void *)tracking, &notification_ref);

        tracking->msg_port = notification_ref;
        IOObjectRelease(battery);
    }
}


static void BatteryInterest(
    void *refcon, 
    io_service_t batt, 
    natural_t messageType, 
    void *messageArgument)
{
    CFArrayRef          battery_info = NULL;
    IOPMBattery         *changed_batt = (IOPMBattery *)refcon;
    IOPMBattery         **batt_stats;

    if(kIOPMMessageBatteryStatusHasChanged == messageType)
    {
        // Update the arbiter
        changed_batt->me = (io_registry_entry_t)batt;
        _batteryChanged(changed_batt);


        // Pass control over to PMUBattery for battery calculation
        batt_stats = _batteries();        
        BatteryTimeRemainingBatteriesHaveChanged(batt_stats);


        // Get legacy battery info & pass control over to PMSettings
        battery_info = isA_CFArray(_copyLegacyBatteryInfo());
        if(!battery_info) return;
        PMSettingsBatteriesHaveChanged(battery_info);

        CFRelease(battery_info);
    }

    return;
}


/* SleepWakeCallback
 * 
 * Receives notifications on system sleep and system wake.
 */
static void
SleepWakeCallback(void * port,io_service_t y,natural_t messageType,void * messageArgument)
{
    // Notify BatteryTimeRemaining
    BatteryTimeRemainingSleepWakeNotification(messageType);

    // Notify PMSettings
    PMSettingsSleepWakeNotification(messageType);
    
    // Notify AutoWake
    AutoWakeSleepWakeNotification(messageType);
    RepeatingAutoWakeSleepWakeNotification(messageType);

    switch ( messageType ) {
    case kIOMessageSystemWillSleep:
        broadcastGMTOffset(); // tell clients what our timezone offset is
    case kIOMessageCanSystemSleep:
        IOAllowPowerChange(_pm_ack_port, (long)messageArgument);
        break;
        
    case kIOMessageSystemHasPoweredOn:
        break;
    }
}

/* ESPrefsHaveChanged
 *
 * Is the handler that configd calls when someone "applies" new Energy Saver
 * Preferences. Since the preferences have probably changed, we re-read them
 * from disk and transmit the new settings to the kernel.
 */
static void 
ESPrefsHaveChanged(SCDynamicStoreRef store, CFArrayRef changedKeys, void *info) 
{
    CFRange   key_range = CFRangeMake(0, CFArrayGetCount(changedKeys));

    if(CFArrayContainsValue(changedKeys, key_range, EnergyPrefsKey))
    {
        // Tell PMSettings that the prefs file has changed
        PMSettingsPrefsHaveChanged();
        PSLowPowerPrefsHaveChanged();
    }

    if(CFArrayContainsValue(changedKeys, key_range, AutoWakePrefsKey))
    {
        // Tell AutoWake that the prefs file has changed
        AutoWakePrefsHaveChanged();
        RepeatingAutoWakePrefsHaveChanged();
    }

    return;
}


/* _ioupsd_exited
 *
 * Gets called (by configd) when /usr/libexec/ioupsd exits
 */
static void _ioupsd_exited(
    pid_t           pid,
    int             status,
    struct rusage   *rusage,
    void            *context)
{
    if(0 != status)
    {
        // ioupsd didn't exit cleanly.
        // Intentionally leave _alreadyRunningIOUPSD set so that we don't 
        // re-launch it.
        syslog(
            LOG_ERR, 
           "PowerManagement: /usr/libexec/ioupsd(%d) has exited with status %d\n", 
           pid, 
           status);
    } else {
        _alreadyRunningIOUPSD = 0;
    }
}


/* UPSDeviceAdded
 *
 * A UPS has been detected running on the system.
 * 
 */
static void UPSDeviceAdded(void *refCon, io_iterator_t iterator)
{
    io_object_t                 upsDevice = MACH_PORT_NULL;
        
    while ( upsDevice = IOIteratorNext(iterator) )
    {
        // If not running, launch the management process ioupsd now.
        if(!_alreadyRunningIOUPSD) {
            char *argv[2] = {"/usr/libexec/ioupsd", NULL};
            pid_t           _ioupsd_pid;

            _alreadyRunningIOUPSD = 1;
            _ioupsd_pid = _SCDPluginExecCommand(&_ioupsd_exited, 0, 0, 0,
                "/usr/libexec/ioupsd", argv);
        }
        IOObjectRelease(upsDevice);
    }
}


/* PowerSourcesHaveChanged
 *
 * Is the handler that gets notified when power source (battery or UPS)
 * state changes. We might respond to this by posting a user notification
 * or performing emergency sleep/shutdown.
 */
extern void
PowerSourcesHaveChanged(void *info) 
{
    CFTypeRef            ps_blob;
    
    ps_blob = isA_CFDictionary(IOPSCopyPowerSourcesInfo());
    if(!ps_blob) return;
    
    // Notifiy PSLowPower of power sources change
    PSLowPowerPSChange(ps_blob);
    
    // Notify PMSettings
    PMSettingsPSChange(ps_blob);
    
    CFRelease(ps_blob);
}

/* timeZoneChangedCallback
 *
 * When our timezone offset changes, tell interested drivers.
 */
static void 
timeZoneChangedCallBack(
    CFNotificationCenterRef center, 
    void *observer, 
    CFStringRef notificationName, 
    const void *object, 
    CFDictionaryRef userInfo)
{

    if( CFEqual(notificationName, gTZNotificationNameString) )
    {
        broadcastGMTOffset();
    }

}


/* broadcastGMTOffset
 *
 * Tell the timezone clients what the seconds offset from GMT is. This info
 * is delivered via the kernel PMSettings interface.
 *
 * Notifications are sent:
 *    - at boot time
 *    - when timezone changes
 *    - at sleep time*
 *    - at display sleep time*
 *
 *    * PM configd does not receive a notification when daylight savings time
 *      changes. In case the system has entered daylight savings time since
 *      boot, we re-broadcast the tz offset at sleep and display sleep.
 */
static void 
broadcastGMTOffset(void)
{
    CFTimeZoneRef               tzr = NULL;
    CFNumberRef                 n = NULL;
    int                         secondsOffset = 0;

    CFTimeZoneResetSystem();
    tzr = CFTimeZoneCopySystem();
    if(!tzr) return;

    secondsOffset = (int)CFTimeZoneGetSecondsFromGMT(tzr, CFAbsoluteTimeGetCurrent());
    n = CFNumberCreate(0, kCFNumberIntType, &secondsOffset);
    if(!n) {
        goto exit;
    }
    
    // Tell the root domain what our timezone's offset from GMT is.
    // IOPMrootdomain will relay the message on to interested PMSetting clients.
    _setRootDomainProperty(CFSTR("TimeZoneOffsetSeconds"), n);

exit:
    if(tzr) CFRelease(tzr);
    if(n) CFRelease(n);
    return;
}

/* displayPowerStateChange
 *
 * displayPowerStateChange gets notified when the display changes power state.
 * Power state changes look like this:
 * (1) Full power -> dim
 * (2) dim -> display sleep
 * (3) display sleep -> display sleep
 * 
 * We're interested in state transition 2. On transition to state 2 we
 * broadcast the system clock's offset from GMT.
 */
static void 
displayPowerStateChange(void *ref, io_service_t service, natural_t messageType, void *arg)
{
    static      int level = 0;
    switch (messageType)
    {
        case kIOMessageDeviceWillPowerOff:
            level++;
            if(2 == level) {
                // Display is transition from dim to full sleep.
                broadcastGMTOffset();            
            }
            break;
            
        case kIOMessageDeviceHasPoweredOn:
            level = 0;
            break;
    }            
}

/* initializeESPrefsDynamicStore
 *
 * Registers a handler that configd calls when someone changes com.apple.PowerManagement.xml
 */
static void
initializeESPrefsDynamicStore(void)
{
    CFRunLoopSourceRef          CFrls = NULL;
    CFMutableArrayRef           watched_keys = NULL;
    SCDynamicStoreRef           energyDS;

    watched_keys = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    if(!watched_keys) return;
    
    energyDS = SCDynamicStoreCreate(NULL, CFSTR(kIOPMAppName), &ESPrefsHaveChanged, NULL);
    if(!energyDS) return;

    // Setup notification for changes in Energy Saver prefences
    EnergyPrefsKey = SCDynamicStoreKeyCreatePreferences(
                                NULL, 
                                CFSTR(kIOPMPrefsPath), 
                                kSCPreferencesKeyApply);
    if(EnergyPrefsKey) 
        CFArrayAppendValue(watched_keys, EnergyPrefsKey);

    // Setup notification for changes in AutoWake prefences
    AutoWakePrefsKey = SCDynamicStoreKeyCreatePreferences(
                NULL, 
                CFSTR(kIOPMAutoWakePrefsPath), 
                kSCPreferencesKeyCommit);
    if(AutoWakePrefsKey) 
        CFArrayAppendValue(watched_keys, AutoWakePrefsKey);

    SCDynamicStoreSetNotificationKeys(energyDS, watched_keys, NULL);

    // Create and add RunLoopSource
    CFrls = SCDynamicStoreCreateRunLoopSource(NULL, energyDS, 0);
    if(CFrls) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), CFrls, kCFRunLoopDefaultMode);    
        CFRelease(CFrls);
    }

    CFRelease(watched_keys);
    CFRelease(energyDS);
    return;
}

/* initializePowerSourceChanges
 *
 * Registers a handler that gets called on power source (battery or UPS) changes
 */
static void
initializePowerSourceChangeNotification(void)
{
    CFRunLoopSourceRef         CFrls;
        
    // Create and add RunLoopSource
    CFrls = IOPSNotificationCreateRunLoopSource(PowerSourcesHaveChanged, NULL);
    if(CFrls) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), CFrls, kCFRunLoopDefaultMode);    
        CFRelease(CFrls);
    }
}



static boolean_t 
pm_mig_demux(
    mach_msg_header_t * request,
    mach_msg_header_t * reply)
{
    mach_dead_name_notification_t *deadRequest = 
                    (mach_dead_name_notification_t *)request;
    boolean_t processed = FALSE;

    mach_msg_format_0_trailer_t * trailer;

    if ((request->msgh_id >= _powermanagement_subsystem.start &&
         request->msgh_id < _powermanagement_subsystem.end)) 
     {

        /*
         * Get the caller's credentials (eUID/eGID) from the message trailer.
         */
        trailer = (mach_msg_security_trailer_t *)((vm_offset_t)request +
            round_msg(request->msgh_size));

        if ((trailer->msgh_trailer_type == MACH_MSG_TRAILER_FORMAT_0) &&
           (trailer->msgh_trailer_size >= MACH_MSG_TRAILER_FORMAT_0_SIZE)) {

            gClientUID = trailer->msgh_sender.val[0];
            gClientGID = trailer->msgh_sender.val[1];
        } else {
            //kextd_error_log("caller's credentials not available");
            gClientUID = -1;
            gClientGID = -1;
        }
    }
    
    processed = powermanagement_server(request, reply);
    if(processed) return true;
    
    // Check for tasks which have exited and clean-up PM assertions
    if(MACH_NOTIFY_DEAD_NAME == request->msgh_id) 
    {
        cleanupAssertions(deadRequest->not_port);
        mach_port_deallocate(mach_task_self(), deadRequest->not_port);

        reply->msgh_bits        = 0;
        reply->msgh_remote_port    = MACH_PORT_NULL;
        return TRUE;
    }

    // mig request is not in our subsystem range!
    // generate error reply packet
    reply->msgh_bits        = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request->msgh_bits), 0);
    reply->msgh_remote_port = request->msgh_remote_port;
    reply->msgh_size        = sizeof(mig_reply_error_t);    /* Minimal size */
    reply->msgh_local_port  = MACH_PORT_NULL;
    reply->msgh_id          = request->msgh_id + 100;
    ((mig_reply_error_t *)reply)->NDR = NDR_record;
    ((mig_reply_error_t *)reply)->RetCode = MIG_BAD_ID;
    
    return processed;
}



static void
mig_server_callback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    mig_reply_error_t * bufRequest = msg;
    mig_reply_error_t * bufReply = CFAllocatorAllocate(
        NULL, _powermanagement_subsystem.maxsize, 0);
    mach_msg_return_t   mr;
    int                 options;

    /* we have a request message */
    (void) pm_mig_demux(&bufRequest->Head, &bufReply->Head);

    if (!(bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) &&
        (bufReply->RetCode != KERN_SUCCESS)) {

        if (bufReply->RetCode == MIG_NO_REPLY) {
            /*
             * This return code is a little tricky -- it appears that the
             * demux routine found an error of some sort, but since that
             * error would not normally get returned either to the local
             * user or the remote one, we pretend it's ok.
             */
            CFAllocatorDeallocate(NULL, bufReply);
            return;
        }

        /*
         * destroy any out-of-line data in the request buffer but don't destroy
         * the reply port right (since we need that to send an error message).
         */
        bufRequest->Head.msgh_remote_port = MACH_PORT_NULL;
        mach_msg_destroy(&bufRequest->Head);
    }

    if (bufReply->Head.msgh_remote_port == MACH_PORT_NULL) {
        /* no reply port, so destroy the reply */
        if (bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) {
            mach_msg_destroy(&bufReply->Head);
        }
        CFAllocatorDeallocate(NULL, bufReply);
        return;
    }

    /*
     * send reply.
     *
     * We don't want to block indefinitely because the client
     * isn't receiving messages from the reply port.
     * If we have a send-once right for the reply port, then
     * this isn't a concern because the send won't block.
     * If we have a send right, we need to use MACH_SEND_TIMEOUT.
     * To avoid falling off the kernel's fast RPC path unnecessarily,
     * we only supply MACH_SEND_TIMEOUT when absolutely necessary.
     */

    options = MACH_SEND_MSG;
    if (MACH_MSGH_BITS_REMOTE(bufReply->Head.msgh_bits) == MACH_MSG_TYPE_MOVE_SEND_ONCE) {
        options |= MACH_SEND_TIMEOUT;
    }
    mr = mach_msg(&bufReply->Head,        /* msg */
              options,            /* option */
              bufReply->Head.msgh_size,    /* send_size */
              0,            /* rcv_size */
              MACH_PORT_NULL,        /* rcv_name */
              MACH_MSG_TIMEOUT_NONE,    /* timeout */
              MACH_PORT_NULL);        /* notify */


    /* Has a message error occurred? */
    switch (mr) {
        case MACH_SEND_INVALID_DEST:
        case MACH_SEND_TIMED_OUT:
            /* the reply can't be delivered, so destroy it */
            mach_msg_destroy(&bufReply->Head);
            break;

        default :
            /* Includes success case.  */
            break;
    }

    CFAllocatorDeallocate(NULL, bufReply);
}

kern_return_t _io_pm_set_active_profile(
                mach_port_t         server,
                vm_offset_t         profiles_ptr,
                mach_msg_type_number_t    profiles_len,
                int                 *result)
{
    void                    *profiles_buf = (void *)profiles_ptr;
    CFDictionaryRef         power_profiles = NULL;
    
    power_profiles = (CFDictionaryRef)IOCFUnserialize(profiles_buf, 0, 0, 0);
    if(isA_CFDictionary(power_profiles)) {
        *result = _IOPMSetActivePowerProfilesRequiresRoot(power_profiles, gClientUID, gClientGID);
        CFRelease(power_profiles);
    } else if(power_profiles) {
        CFRelease(power_profiles);
    }

    // deallocate client's memory
    vm_deallocate(mach_task_self(), (vm_address_t)profiles_ptr, profiles_len);

    return KERN_SUCCESS;
}

static CFStringRef              
serverMPCopyDescription(const void *info)
{
        return CFStringCreateWithFormat(NULL, NULL, CFSTR("<io pm server MP>"));
}
 
static void
initializeMIGServer(void)
{
    kern_return_t           kern_result = 0;
    CFMachPortRef           cf_mach_port = 0;
    CFRunLoopSourceRef      cfmp_rls = 0;
    mach_port_t             our_port;

    CFMachPortContext       context  = { 0, (void *)1, NULL, NULL, serverMPCopyDescription };

    cf_mach_port = CFMachPortCreate(0, mig_server_callback, &context, 0);
    if(!cf_mach_port) {
        goto bail;
    }
    our_port = CFMachPortGetPort(cf_mach_port);
    cfmp_rls = CFMachPortCreateRunLoopSource(0, cf_mach_port, 0);
    if(!cfmp_rls) {
        goto bail;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), cfmp_rls, kCFRunLoopDefaultMode);

    kern_result = bootstrap_register(
                        bootstrap_port, 
                        kIOPMServerBootstrapName, 
                        our_port);

    switch (kern_result) {
      case BOOTSTRAP_SUCCESS:
        break;
      case BOOTSTRAP_NOT_PRIVILEGED:
        break;
      case BOOTSTRAP_SERVICE_ACTIVE:
        break;
      default:
        //syslog(LOG_INFO, "pmconfigd exit: undefined mig error");
        break;
    }

    serverMachPort = cf_mach_port;

bail:
    if(cfmp_rls) CFRelease(cfmp_rls);
    return;
}

/* initializeInteresteNotifications
 *
 * Sets up the notification of general interest from the PMU & RootDomain
 */
static void
initializeInterestNotifications()
{
    IONotificationPortRef       notify_port = 0;
    io_object_t                 notification_ref = 0;
    io_service_t                pmu_service_ref = 0;
    io_iterator_t               battery_iter = 0;
    CFRunLoopSourceRef          rlser = 0;
    IOReturn                    ret;
    
    CFMutableDictionaryRef      matchingDict = 0;
    CFMutableDictionaryRef      propertyDict = 0;
    kern_return_t               kr;
    
    /* Notifier */
    notify_port = IONotificationPortCreate(0);
    rlser = IONotificationPortGetRunLoopSource(notify_port);
    if(!rlser) goto finish;
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rlser, kCFRunLoopDefaultMode);

    /* PMU */
    pmu_service_ref = IOServiceGetMatchingService(0, IOServiceNameMatching("ApplePMU"));
    if(!pmu_service_ref) goto battery;

    ret = IOServiceAddInterestNotification(notify_port, pmu_service_ref, 
                                kIOGeneralInterest, PMUInterest,
                                0, &notification_ref);
    if(kIOReturnSuccess != ret) goto battery;

battery:

    // Get match notification on IOPMPowerSource
    kr = IOServiceAddMatchingNotification(
                                notify_port,
                                kIOFirstMatchNotification,
                                IOServiceMatching("IOPMPowerSource"),
                                BatteryMatch,
                                (void *)notify_port,
                                &battery_iter);
    if(KERN_SUCCESS != kr) goto ups;

    // Install notifications on existing instances.
    BatteryMatch((void *)notify_port, battery_iter);
    
ups:

    matchingDict = IOServiceMatching(kIOServiceClass); 
    if (!matchingDict) goto finish;
            
    propertyDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 
                    0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!propertyDict) goto finish;        

    // We are only interested in devices that have kIOUPSDeviceKey property set
    CFDictionarySetValue(propertyDict, CFSTR(kIOUPSDeviceKey), kCFBooleanTrue);    
    CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyDict);
    
    // Now set up a notification to be called when a device is first matched by I/O Kit.
    // Note that this will not catch any devices that were already plugged in so we take
    // care of those later.
    kr = IOServiceAddMatchingNotification(
                              notify_port, 
                              kIOFirstMatchNotification, 
                              matchingDict,
                              UPSDeviceAdded,
                              NULL,
                              &_ups_added_noteref);

    matchingDict = 0; // reference consumed by AddMatchingNotification
    if ( kr != kIOReturnSuccess ) goto finish;

    // Check for existing matching devices and launch ioupsd if present.
    UPSDeviceAdded( NULL, _ups_added_noteref);        

finish:   
    if(rlser) CFRelease(rlser);
    if(pmu_service_ref) IOObjectRelease(pmu_service_ref);
    if(matchingDict) CFRelease(matchingDict);
    if(propertyDict) CFRelease(propertyDict);
    return;
}

/* initializeTimezoneChangeNotifications
 *
 * Sets up the tz notifications that we re-broadcast to all interested
 * kernel clients listening via PMSettings
 */
static void
initializeTimezoneChangeNotifications(void)
{
    CFNotificationCenterRef         distNoteCenter = NULL;
    
    gTZNotificationNameString = CFStringCreateWithCString(
                        kCFAllocatorDefault,
                        "NSSystemTimeZoneDidChangeDistributedNotification",
                        kCFStringEncodingMacRoman);

    distNoteCenter = CFNotificationCenterGetDistributedCenter();
    if(distNoteCenter)
    {
        CFNotificationCenterAddObserver(
                       distNoteCenter, 
                       NULL, 
                       timeZoneChangedCallBack, 
                       gTZNotificationNameString,
                       NULL, 
                       CFNotificationSuspensionBehaviorDeliverImmediately);
    }

    // Boot time - tell clients what our timezone offset is
    broadcastGMTOffset(); 
}


/* intializeDisplaySleepNotifications
 *
 * Notifications on display sleep. We tell the tz+dst offset to our timezone 
 * clients when display sleep kicks in. 
 */
static void
intializeDisplaySleepNotifications(void)
{
    IONotificationPortRef       note_port = MACH_PORT_NULL;
    CFRunLoopSourceRef          dimSrc = NULL;
    io_service_t                display_wrangler = MACH_PORT_NULL;
    io_object_t                 dimming_notification_object = MACH_PORT_NULL;
    IOReturn                    ret;

    display_wrangler = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceNameMatching("IODisplayWrangler"));
    if(!display_wrangler) return;
    
    note_port = IONotificationPortCreate(MACH_PORT_NULL);
    if(!note_port) goto exit;
    
    ret = IOServiceAddInterestNotification(note_port, display_wrangler, 
                kIOGeneralInterest, displayPowerStateChange,
                NULL, &dimming_notification_object);
    if(ret != kIOReturnSuccess) goto exit;
    
    dimSrc = IONotificationPortGetRunLoopSource(note_port);
    
    if(dimSrc)
    {
        CFRunLoopAddSource(CFRunLoopGetCurrent(), dimSrc, kCFRunLoopDefaultMode);
    }
    
exit:
    // Do not release dimming_notification_object, would uninstall notification
    // Do not release dimSrc because it's 'owned' by note_port, and both
    // must be kept around to receive this notification
    if(MACH_PORT_NULL != display_wrangler) IOObjectRelease(display_wrangler);
}

// use 'make' to build standalone debuggable executable 'pm'

#ifdef  STANDALONE
int
main(int argc, char **argv)
{
    openlog("pmcfgd", LOG_PID | LOG_NDELAY, LOG_DAEMON);

    load(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);

    prime();

    CFRunLoopRun();

    /* not reached */
    exit(0);
    return 0;
}
#endif

