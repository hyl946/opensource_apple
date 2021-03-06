/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 * Modification History
 *
 * August 5, 2004			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include "scutil.h"
#include "net.h"
#include "net_protocol.h"


#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>


/* -------------------- */


__private_extern__
CFComparisonResult
_compare_protocols(const void *val1, const void *val2, void *context)
{
	SCNetworkProtocolRef	p1	= (SCNetworkProtocolRef)val1;
	SCNetworkProtocolRef	p2	= (SCNetworkProtocolRef)val2;
	CFStringRef		type1;
	CFStringRef		type2;

	type1 = SCNetworkProtocolGetProtocolType(p1);
	type2 = SCNetworkProtocolGetProtocolType(p2);

	return CFStringCompare(type1, type2, 0);
}


static CFStringRef
__copyIPv4Address(const char *arg)
{
	char			buf[128];
	struct sockaddr_in	sin;

	bzero(&sin, sizeof(sin));
	sin.sin_len    = sizeof(sin);
	sin.sin_family = AF_INET;
	if (inet_aton(arg, &sin.sin_addr) != 1) {
		return NULL;
	}

	_SC_sockaddr_to_string((struct sockaddr *)&sin, buf, sizeof(buf));
	return CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
}


static CFStringRef
__copyIPv6Address(const char *arg)
{
	char			buf[128];
	char			*p;
	struct sockaddr_in6	sin6;

	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len    = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, arg, &sin6.sin6_addr) != 1) {
		return NULL;
	}

	p = strchr(arg, '%');
	if (p != NULL) {
		sin6.sin6_scope_id = if_nametoindex(p+1);
	}

	_SC_sockaddr_to_string((struct sockaddr *)&sin6, buf, sizeof(buf));
	return CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8);
}


/* -------------------- */


static SCNetworkProtocolRef
_find_protocol(char *match)
{
	Boolean			allowIndex	= TRUE;
	CFIndex			i;
	CFIndex			n;
	CFStringRef		select_name	= NULL;
	SCNetworkProtocolRef	selected	= NULL;

	if (protocols == NULL) {
		if (net_service == NULL) {
			SCPrint(TRUE, stdout, CFSTR("network service not selected\n"));
			return NULL;
		}

		protocols = SCNetworkServiceCopyProtocols(net_service);
		if (protocols == NULL) {
			SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
			return NULL;
		}

		n = CFArrayGetCount(protocols);
		if (n > 1) {
			CFMutableArrayRef	sorted;

			sorted = CFArrayCreateMutableCopy(NULL, 0, protocols);
			CFArraySortValues(sorted,
					  CFRangeMake(0, n),
					  _compare_protocols,
					  NULL);
			CFRelease(protocols);
			protocols = sorted;
		}

		allowIndex = FALSE;
	}

	// try to select the protocol by its protocol type

	select_name = CFStringCreateWithCString(NULL, match, kCFStringEncodingUTF8);

	n = CFArrayGetCount(protocols);
	for (i = 0; i < n; i++) {
		SCNetworkProtocolRef	protocol;
		CFStringRef		type;

		protocol = CFArrayGetValueAtIndex(protocols, i);
		type     = SCNetworkProtocolGetProtocolType(protocol);
		if (CFStringCompare(select_name, type, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
			selected = protocol;
			goto done;
		}
	}

	if (allowIndex) {
		char	*end;
		char	*str	= match;
		long	val;

		// try to select the protocol by its index

		errno = 0;
		val = strtol(str, &end, 10);
		if ((*str != '\0') && (*end == '\0') && (errno == 0)) {
			if ((val > 0) && (val <= n)) {
				selected = CFArrayGetValueAtIndex(protocols, val - 1);
			}
		}
	}

	if (selected != NULL) {
		goto done;
	}

	SCPrint(TRUE, stdout, CFSTR("no match, which protocol?\n"));

    done :

	if (select_name != NULL) CFRelease(select_name);
	return selected;
}


/* -------------------- */


__private_extern__
void
create_protocol(int argc, char **argv)
{
	SCNetworkInterfaceRef	interface;
	CFStringRef		protocolType;

	if ((argc < 1) || (strlen(argv[0]) == 0)) {
		SCPrint(TRUE, stdout, CFSTR("what protocol type?\n"));
		return;
	}

	if (net_service == NULL) {
		SCPrint(TRUE, stdout, CFSTR("network service not selected\n"));
		return;
	}

	protocolType = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);

	interface = SCNetworkServiceGetInterface(net_service);
	if (interface != NULL) {
		CFArrayRef	supported;
		CFIndex		i;
		CFIndex		n;

		supported = SCNetworkInterfaceGetSupportedProtocolTypes(interface);
		n = (supported != NULL) ? CFArrayGetCount(supported) : 0;
		for (i = 0; i < n; i++) {
			CFStringRef	supportedType;

			supportedType = CFArrayGetValueAtIndex(supported, i);
			if (CFStringCompare(protocolType,
					    supportedType,
					    kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
				CFRelease(protocolType);
				protocolType = CFRetain(supportedType);
				break;
			}
		}
	}

	if (!SCNetworkServiceAddProtocolType(net_service, protocolType)) {
		SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
		goto done;
	}

	net_changed = TRUE;

	if (protocols != NULL) {
		CFRelease(protocols);
		protocols = NULL;
	}

	if (net_protocol != NULL) CFRelease(net_protocol);
//	net_protocol = NULL;

	net_protocol = SCNetworkServiceCopyProtocol(net_service, protocolType);
	if (net_protocol == NULL) {
		SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
		goto done;
	}

	SCPrint(TRUE, stdout,
		CFSTR("protocol \"%@\" selected\n"),
		protocolType);

    done :

	CFRelease(protocolType);
	return;
}


/* -------------------- */


__private_extern__
void
disable_protocol(int argc, char **argv)
{
	SCNetworkProtocolRef	protocol	= NULL;

	if (argc > 0) {
		protocol = _find_protocol(argv[0]);
	} else {
		if (net_protocol != NULL) {
			protocol = net_protocol;
		} else {
			SCPrint(TRUE, stdout, CFSTR("protocol not selected\n"));
			return;
		}
	}

	if (protocol == NULL) {
		return;
	}

	if (!SCNetworkProtocolSetEnabled(protocol, FALSE)) {
		SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
		return;
	}

	net_changed = TRUE;

	return;
}


/* -------------------- */


__private_extern__
void
enable_protocol(int argc, char **argv)
{
	SCNetworkProtocolRef	protocol	= NULL;

	if (argc > 0) {
		protocol = _find_protocol(argv[0]);
	} else {
		if (net_protocol != NULL) {
			protocol = net_protocol;
		} else {
			SCPrint(TRUE, stdout, CFSTR("protocol not selected\n"));
			return;
		}
	}

	if (protocol == NULL) {
		return;
	}

	if (!SCNetworkProtocolSetEnabled(protocol, TRUE)) {
		SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
		return;
	}

	net_changed = TRUE;

	return;
}


/* -------------------- */


__private_extern__
void
remove_protocol(int argc, char **argv)
{
	SCNetworkProtocolRef	protocol	= NULL;
	CFStringRef		protocolType;

	if (argc > 0) {
		protocol = _find_protocol(argv[0]);
	} else {
		if (net_protocol != NULL) {
			protocol = net_protocol;
		} else {
			SCPrint(TRUE, stdout, CFSTR("protocol not selected\n"));
			return;
		}
	}

	if (protocol == NULL) {
		return;
	}

	CFRetain(protocol);

	protocolType = SCNetworkProtocolGetProtocolType(protocol);
	if (!SCNetworkServiceRemoveProtocolType(net_service, protocolType)) {
		SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
		goto done;
	}

	net_changed = TRUE;

	SCPrint(TRUE, stdout,
		CFSTR("protocol \"%@\" removed\n"),
		protocolType);

	if ((net_protocol != NULL) && CFEqual(protocol, net_protocol)) {
		CFRelease(net_protocol);
		net_protocol = NULL;
		SCPrint(TRUE, stdout, CFSTR("& no protocol selected\n"));
	}

	if (protocols != NULL) {
		CFRelease(protocols);
		protocols = NULL;
	}

    done :

	CFRelease(protocol);
	return;
}


/* -------------------- */


__private_extern__
void
select_protocol(int argc, char **argv)
{
	SCNetworkProtocolRef	protocol;

	protocol = _find_protocol(argv[0]);

	if (protocol == NULL) {
		return;
	}

	if (net_protocol != NULL) CFRelease(net_protocol);
	net_protocol = CFRetain(protocol);

	SCPrint(TRUE, stdout,
		CFSTR("protocol \"%@\" selected\n"),
		SCNetworkProtocolGetProtocolType(protocol));

	return;
}


/* -------------------- */


static selections appletalkConfigMethods[] = {
	{ CFSTR("node")      , &kSCValNetAppleTalkConfigMethodNode      , 0 },
	{ CFSTR("router")    , &kSCValNetAppleTalkConfigMethodRouter    , 0 },
	{ CFSTR("seedrouter"), &kSCValNetAppleTalkConfigMethodSeedRouter, 0 },
	{ NULL               , NULL                                     , 0 }
};


static int
__doAppleTalkConfigMethod(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	CFStringRef	configMethod;

	configMethod = CFDictionaryGetValue(newConfiguration, key);
	if (!CFEqual(key, kSCValNetAppleTalkConfigMethodSeedRouter)) {
		CFDictionaryRemoveValue(newConfiguration, kSCPropNetAppleTalkSeedZones);
		CFDictionaryRemoveValue(newConfiguration, kSCPropNetAppleTalkSeedNetworkRange);
	}

	return 0;
}


static int
__doAppleTalkNetworkRange(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	if (argc < 1) {
		SCPrint(TRUE, stdout, CFSTR("network range not specified\n"));
		return -1;
	}

	if (strlen(argv[0]) > 0) {
		CFArrayRef	array;
		char		*cp;
		CFNumberRef	range[2];

		range[0] = _copy_number(argv[0]);
		if (range[0] == NULL) {
			SCPrint(TRUE, stdout, CFSTR("invalid start of range\n"));
			return -1;
		}

		cp = strchr(argv[0], '-');
		if (cp == NULL) {
			range[1] = _copy_number(cp);
			if (range[1] == NULL) {
				CFRelease(range[0]);
				SCPrint(TRUE, stdout, CFSTR("invalid end of range\n"));
				return -1;
			}
		} else {
			range[1] = CFRetain(range[0]);
		}

		array = CFArrayCreate(NULL,
				      (const void **)range,
				      sizeof(range)/sizeof(range[0]),
				      &kCFTypeArrayCallBacks);
		CFRelease(range[0]);
		CFRelease(range[1]);

		CFDictionarySetValue(newConfiguration, key, array);
		CFRelease(array);
	} else {
		CFDictionaryRemoveValue(newConfiguration, key);
	}

	return 1;
}


static options appletalkOptions[] = {
	{ "ConfigMethod"    , "configuration method"
				       , isChooseOne  , &kSCPropNetAppleTalkConfigMethod    , __doAppleTalkConfigMethod, (void *)appletalkConfigMethods },
	{   "config"        , "configuration method"
				       , isChooseOne  , &kSCPropNetAppleTalkConfigMethod    , __doAppleTalkConfigMethod, (void *)appletalkConfigMethods },
	{ "DefaultZone"     , "zone"   , isString     , &kSCPropNetAppleTalkDefaultZone     , NULL                     , NULL                           },
	{ "NodeID"          , "node"   , isNumber     , &kSCPropNetAppleTalkNodeID          , NULL                     , NULL                           },
	{ "NetworkID"       , "network", isNumber     , &kSCPropNetAppleTalkNetworkID       , NULL                     , NULL                           },
	{ "SeedNetworkRange", "range"  , isOther      , &kSCPropNetAppleTalkSeedNetworkRange, __doAppleTalkNetworkRange, NULL                           },
	{ "SeedZones"       , "zone"   , isStringArray, &kSCPropNetAppleTalkSeedZones       , NULL                     , NULL                           },

	{ "?"               , NULL   , isHelp       , NULL                                , NULL                     ,
	    "\nAppleTalk configuration commands\n\n"
	    " set protocol config {Node|Router|SeedRouter}\n"
	    " set protocol defaultzone zone\n"
	    " set protocol node id\n"
	    " set protocol network id\n"
	    "\n w/config=Node\n"
	    "   None\n"
	    "\n w/config=Router\n"
	    "   None\n"
	    "\n w/config=SeedRouter\n"
	    "   set protocol seednetworkrange low[-high]\n"
	    "   set protocol seedzones zone[,zone-2]\n"
	}
};
#define	N_APPLETALK_OPTIONS	(sizeof(appletalkOptions) / sizeof(appletalkOptions[0]))


static Boolean
set_protocol_appletalk(int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	Boolean	ok;

	ok = _process_options(appletalkOptions, N_APPLETALK_OPTIONS, argc, argv, newConfiguration);
	return ok;
}


/* -------------------- */


static CFStringRef
__cleanupDomainName(CFStringRef domain)
{
	CFMutableStringRef	newDomain;

	newDomain = CFStringCreateMutableCopy(NULL, 0, domain);
	CFStringTrimWhitespace(newDomain);
	CFStringTrim(newDomain, CFSTR("."));
	if (CFStringGetLength(newDomain) == 0) {
		CFRelease(newDomain);
		newDomain = NULL;
	}

	return newDomain;
}


static int
__doDNSDomain(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	if (argc < 1) {
		SCPrint(TRUE, stdout, CFSTR("DNS domain name not specified\n"));
		return -1;
	}

	if (strlen(argv[0]) > 0) {
		CFStringRef	domain;
		CFStringRef	str;

		str = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
		domain = __cleanupDomainName(str);
		CFRelease(str);

		if (domain != NULL) {
			CFDictionarySetValue(newConfiguration, key, domain);
			CFRelease(domain);
		} else {
			SCPrint(TRUE, stdout, CFSTR("invalid DNS domain name\n"));
			return -1;
		}
	} else {
		CFDictionaryRemoveValue(newConfiguration, key);
	}

	return 1;
}


static int
__doDNSDomainArray(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	CFMutableArrayRef	domains;

	if (argc < 1) {
		SCPrint(TRUE, stdout, CFSTR("DNS search domain name(s) not specified\n"));
		return -1;
	}

	domains = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (strlen(argv[0]) > 0) {
		CFArrayRef	array;
		CFStringRef	str;

		str = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
		array = CFStringCreateArrayBySeparatingStrings(NULL, str, CFSTR(","));
		CFRelease(str);

		if (array != NULL) {
			CFIndex	i;
			CFIndex	n	= CFArrayGetCount(array);

			for (i = 0; i < n; i++) {
				CFStringRef	domain;

				domain = __cleanupDomainName(CFArrayGetValueAtIndex(array, i));
				if (domain != NULL) {
					CFArrayAppendValue(domains, domain);
					CFRelease(domain);
				} else {
					CFRelease(array);
					CFRelease(domains);
					SCPrint(TRUE, stdout, CFSTR("invalid DNS search domain name\n"));
					return -1;
				}
			}
			CFRelease(array);
		}
	}

	if (CFArrayGetCount(domains) > 0) {
		CFDictionarySetValue(newConfiguration, key, domains);
	} else {
		CFDictionaryRemoveValue(newConfiguration, key);
	}

	CFRelease(domains);
	return 1;
}


static int
__doDNSServerAddresses(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	CFMutableArrayRef	servers;

	if (argc < 1) {
		SCPrint(TRUE, stdout, CFSTR("DNS search domain name(s) not specified\n"));
		return -1;
	}

	servers = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (strlen(argv[0]) > 0) {
		CFArrayRef	array;
		CFIndex		i;
		CFIndex		n;
		CFStringRef	str;

		str = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
		array = CFStringCreateArrayBySeparatingStrings(NULL, str, CFSTR(","));
		CFRelease(str);

		n = (array != NULL) ? CFArrayGetCount(array) : 0;
		for (i = 0; i < n; i++) {
			char	str[32];

			if (_SC_cfstring_to_cstring(CFArrayGetValueAtIndex(array, i),
						    str,
						    sizeof(str),
						    kCFStringEncodingUTF8) != NULL) {
				CFStringRef	server;

				server = __copyIPv4Address(str);
				if (server == NULL) {
					server = __copyIPv6Address(str);
				}
				if (server != NULL) {
					CFArrayAppendValue(servers, server);
					CFRelease(server);
					continue;
				}
			}

			SCPrint(TRUE, stdout, CFSTR("invalid DNS name server address\n"));
			CFRelease(array);
			CFRelease(servers);
			return -1;
		}
		if (array != NULL) CFRelease(array);
	}

	if (CFArrayGetCount(servers) > 0) {
		CFDictionarySetValue(newConfiguration, key, servers);
	} else {
		CFDictionaryRemoveValue(newConfiguration, key);
	}

	CFRelease(servers);
	return 1;
}


static options dnsOptions[] = {
	{ "DomainName"     , "domain" , isOther, &kSCPropNetDNSDomainName     , __doDNSDomain         , NULL },
	{   "domain"       , "domain" , isOther, &kSCPropNetDNSDomainName     , __doDNSDomain         , NULL },
	{ "SearchDomains"  , "search" , isOther, &kSCPropNetDNSSearchDomains  , __doDNSDomainArray    , NULL },
	{   "search"       , "search" , isOther, &kSCPropNetDNSSearchDomains  , __doDNSDomainArray    , NULL },
	{ "ServerAddresses", "address", isOther, &kSCPropNetDNSServerAddresses, __doDNSServerAddresses, NULL },
	{   "nameserver"   , "address", isOther, &kSCPropNetDNSServerAddresses, __doDNSServerAddresses, NULL },
	{   "nameservers"  , "address", isOther, &kSCPropNetDNSServerAddresses, __doDNSServerAddresses, NULL },

	{ "?"              , NULL     , isHelp , NULL                         , NULL,
	    "\nDNS configuration commands\n\n"
	    " set protocol search domain-name[,domain-name-2]\n"
	    " set protocol nameserver x1.x1.x1.x1[,x2.x2.x2.x2]"
	}
};
#define	N_DNS_OPTIONS	(sizeof(dnsOptions) / sizeof(dnsOptions[0]))


static Boolean
set_protocol_dns(int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	Boolean	ok;

	ok = _process_options(dnsOptions, N_DNS_OPTIONS, argc, argv, newConfiguration);
	return ok;
}


/* -------------------- */


#define allowIPv4Address	1<<1	// allow address
#define allowIPv4Netmask	1<<2	// allow subnet mask
#define allowIPv4Router		1<<3	// allow router
#define allowIPv4DHCPClientID	1<<4	// allow DCHP Client ID

static selections ipv4ConfigMethods[] = {
	{ CFSTR("BOOTP")    , &kSCValNetIPv4ConfigMethodBOOTP    , 0                                                 },
	{ CFSTR("DHCP")     , &kSCValNetIPv4ConfigMethodDHCP     , allowIPv4DHCPClientID                             },
	{ CFSTR("INFORM")   , &kSCValNetIPv4ConfigMethodINFORM   , allowIPv4Address                                  },
	{ CFSTR("LinkLocal"), &kSCValNetIPv4ConfigMethodLinkLocal, 0                                                 },
	{ CFSTR("Manual")   , &kSCValNetIPv4ConfigMethodManual   , allowIPv4Address|allowIPv4Netmask|allowIPv4Router },
	{ CFSTR("PPP")      , &kSCValNetIPv4ConfigMethodPPP      , allowIPv4Address|selectionNotAvailable            },
	{ NULL              , NULL                               , 0                                                 }
};


static int
__doIPv4ConfigMethod(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	unsigned int	flags;
	CFStringRef	method;
	CFIndex		methodIndex;

	method = CFDictionaryGetValue(newConfiguration, key);
	methodIndex = _find_selection(method, (selections *)ipv4ConfigMethods, &flags);
	if (methodIndex != kCFNotFound) {
		if (!(flags & allowIPv4Address)) {
			CFDictionaryRemoveValue(newConfiguration, kSCPropNetIPv4Addresses);
		}
		if (!(flags & allowIPv4Netmask)) {
			CFDictionaryRemoveValue(newConfiguration, kSCPropNetIPv4SubnetMasks);
		}
		if (!(flags & allowIPv4Router)) {
			CFDictionaryRemoveValue(newConfiguration, kSCPropNetIPv4Router);
		}
		if (!(flags & allowIPv4DHCPClientID)) {
			CFDictionaryRemoveValue(newConfiguration, kSCPropNetIPv4DHCPClientID);
		}
	} else {
		SCPrint(TRUE, stdout, CFSTR("unknown configuration method\n"));
		return -1;
	}

	return 0;
}


static int
__doIPv4Addresses(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	Boolean	useArray	= (info == (void *)FALSE) ? FALSE : TRUE;

	if (strlen(argv[0]) > 0) {
		CFStringRef	address;

		address = __copyIPv4Address(argv[0]);
		if (address != NULL) {
			if (useArray) {
				CFArrayRef	addresses;

				addresses = CFArrayCreate(NULL, (const void **)&address, 1, &kCFTypeArrayCallBacks);
				CFDictionarySetValue(newConfiguration, key, addresses);
				CFRelease(addresses);
			} else {
				CFDictionarySetValue(newConfiguration, key, address);
			}
			CFRelease(address);
		} else {
			return -1;
		}
	} else {
		CFDictionaryRemoveValue(newConfiguration, key);
	}

	return 1;
}


static options ipv4Options[] = {
	{ "ConfigMethod", "configuration method"
				     , isChooseOne   , &kSCPropNetIPv4ConfigMethod, __doIPv4ConfigMethod, (void *)ipv4ConfigMethods },
	{   "config"    , "configuration method"
				     , isChooseOne   , &kSCPropNetIPv4ConfigMethod, __doIPv4ConfigMethod, (void *)ipv4ConfigMethods },
	{ "Addresses"   , "address"  , isOther       , &kSCPropNetIPv4Addresses   , __doIPv4Addresses   , (void *)TRUE              },
	{   "address"   , "address"  , isOther       , &kSCPropNetIPv4Addresses   , __doIPv4Addresses   , (void *)TRUE              },
	{ "SubnetMasks" , "netmask"  , isOther       , &kSCPropNetIPv4SubnetMasks , __doIPv4Addresses   , (void *)TRUE              },
	{   "netmask"   , "netmask"  , isOther       , &kSCPropNetIPv4SubnetMasks , __doIPv4Addresses   , (void *)TRUE              },
	{ "Router"      , "address"  , isOther       , &kSCPropNetIPv4Router      , __doIPv4Addresses   , (void *)FALSE             },
	{ "DHCPClientID", "client ID", isString      , &kSCPropNetIPv4DHCPClientID, NULL                , NULL                      },

	{ "?"           , NULL       , isHelp        , NULL                       , NULL                ,
	    "\nIPv4 configuration commands\n\n"
	    " set protocol config {BOOTP|DHCP|INFORM|MANUAL}\n"
	    "\n w/config=BOOTP\n"
	    "   None\n"
	    "\n w/config=DHCP\n"
	    "   set protocol dhcpclientid identifier\n"
	    "\n w/config=INFORM\n"
	    "   set protocol address x.x.x.x\n"
	    "\n w/config=MANUAL\n"
	    "   set protocol address x.x.x.x\n"
	    "   set protocol netmask x.x.x.x\n"
	    "   set protocol router  x.x.x.x\n"
	}
};
#define	N_IPV4_OPTIONS	(sizeof(ipv4Options) / sizeof(ipv4Options[0]))


static Boolean
set_protocol_ipv4(int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	Boolean	ok;

	ok = _process_options(ipv4Options, N_IPV4_OPTIONS, argc, argv, newConfiguration);
	if (ok) {
		unsigned int	flags;
		CFStringRef	method;
		CFIndex		methodIndex;

		// validate configuration
		method = CFDictionaryGetValue(newConfiguration, kSCPropNetIPv4ConfigMethod);
		methodIndex = _find_selection(method, (selections *)ipv4ConfigMethods, &flags);
		if (methodIndex == kCFNotFound) {
			SCPrint(TRUE, stdout, CFSTR("unknown configuration method\n"));
			return FALSE;
		}

		if (!(flags & allowIPv4Address) && CFDictionaryContainsKey(newConfiguration, kSCPropNetIPv4Addresses)) {
			SCPrint(TRUE, stdout,
				CFSTR("IP address not allowed with %@ configuration\n"),
				ipv4ConfigMethods[methodIndex].selection);
			return FALSE;
		}

		if (!(flags & allowIPv4Netmask) && CFDictionaryContainsKey(newConfiguration, kSCPropNetIPv4SubnetMasks)) {
			SCPrint(TRUE, stdout,
				CFSTR("Subnet mask not allowed with %@ configuration\n"),
				ipv4ConfigMethods[methodIndex].selection);
			return FALSE;
		}

		if (!(flags & allowIPv4Router) && CFDictionaryContainsKey(newConfiguration, kSCPropNetIPv4Router)) {
			SCPrint(TRUE, stdout,
				CFSTR("Default route not allowed with %@ configuration\n"),
				ipv4ConfigMethods[methodIndex].selection);
			return FALSE;
		}

		if (!(flags & allowIPv4DHCPClientID) && CFDictionaryContainsKey(newConfiguration, kSCPropNetIPv4DHCPClientID)) {
			SCPrint(TRUE, stdout,
				CFSTR("DHCP client ID not allowed with %@ configuration\n"),
				ipv4ConfigMethods[methodIndex].selection);
			return FALSE;
		}
	}

	return ok;
}


/* -------------------- */


#define allowIPv6Address	1<<1	// allow address
#define allowIPv6PrefixLength	1<<2	// allow prefix length
#define allowIPv6Router		1<<3	// allow router

static selections ipv6ConfigMethods[] = {
	{ CFSTR("Automatic")          , & kSCValNetIPv6ConfigMethodAutomatic          , 0                                                      },
	{ CFSTR("Manual")             , & kSCValNetIPv6ConfigMethodManual             , allowIPv6Address|allowIPv6PrefixLength|allowIPv6Router },
	{ CFSTR("RouterAdvertisement"), & kSCValNetIPv6ConfigMethodRouterAdvertisement, allowIPv6Address                                       },
	{ CFSTR("6to4")               , & kSCValNetIPv6ConfigMethod6to4               , 0                                                      },
	{ NULL                        , NULL                                          , 0                                                      }
};


static int
__doIPv6ConfigMethod(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	unsigned int	flags;
	CFStringRef	method;
	CFIndex		methodIndex;

	method = CFDictionaryGetValue(newConfiguration, key);
	methodIndex = _find_selection(method, (selections *)ipv6ConfigMethods, &flags);
	if (methodIndex != kCFNotFound) {
		if (!(flags & allowIPv6Address)) {
			CFDictionaryRemoveValue(newConfiguration, kSCPropNetIPv6Addresses);
		}
		if (!(flags & allowIPv6PrefixLength)) {
			CFDictionaryRemoveValue(newConfiguration, kSCPropNetIPv6PrefixLength);
		}
		if (!(flags & allowIPv6Router)) {
			CFDictionaryRemoveValue(newConfiguration, kSCPropNetIPv6Router);
		}
	} else {
		SCPrint(TRUE, stdout, CFSTR("unknown configuration method\n"));
		return -1;
	}

	return 0;
}


static int
__doIPv6Addresses(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	Boolean	useArray	= (info == (void *)FALSE) ? FALSE : TRUE;

	if (strlen(argv[0]) > 0) {
		CFStringRef	address;

		address = __copyIPv6Address(argv[0]);
		if (address != NULL) {
			if (useArray) {
				CFArrayRef	addresses;

				addresses = CFArrayCreate(NULL, (const void **)&address, 1, &kCFTypeArrayCallBacks);
				CFDictionarySetValue(newConfiguration, key, addresses);
				CFRelease(addresses);
			} else {
				CFDictionarySetValue(newConfiguration, key, address);
			}
			CFRelease(address);
		} else {
			return -1;
		}
	} else {
		CFDictionaryRemoveValue(newConfiguration, key);
	}

	return 1;
}


static int
__doIPv6PrefixLength(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	CFNumberRef	num;
	int		prefixLength;

	num = CFDictionaryGetValue(newConfiguration, kSCPropNetPPPAuthPasswordEncryption);
	if (isA_CFNumber(num) &&
	    CFNumberGetValue(num, kCFNumberIntType, &prefixLength) &&
	    (prefixLength >= 0) && (prefixLength <= (sizeof(struct in6_addr) * 8))) {
		return 0;
	}

	return -1;
}


static options ipv6Options[] = {
	{ "ConfigMethod", "configuration method"
					 , isChooseOne, &kSCPropNetIPv6ConfigMethod, __doIPv6ConfigMethod, (void *)ipv6ConfigMethods },
	{   "config"    , "configuration method"
					 , isChooseOne, &kSCPropNetIPv6ConfigMethod, __doIPv6ConfigMethod, (void *)ipv6ConfigMethods },
	{ "Addresses"   , "address"      , isOther    , &kSCPropNetIPv6Addresses   , __doIPv6Addresses   , (void *)TRUE              },
	{   "address"   , "address"      , isOther    , &kSCPropNetIPv6Addresses   , __doIPv6Addresses   , (void *)TRUE              },
	{ "PrefixLength", "prefix length", isNumber   , &kSCPropNetIPv6PrefixLength, __doIPv6PrefixLength, NULL                      },
	{ "Router"      , "address"      , isOther    , &kSCPropNetIPv6Router      , __doIPv6Addresses   , (void *)FALSE             },

	{ "?"           , NULL           , isHelp     , NULL                       , NULL                ,
	    "\nIPv6 configuration commands\n\n"
	    " set protocol config {Automatic|MANUAL}\n"
	    "\n w/config=Automatic\n"
	    "   None\n"
	    "\n w/config=MANUAL\n"
	    "   set protocol address x:x:x:x:x:x\n"
	    "   set protocol router  x:x:x:x:x:x\n"
	    "   set protocol prefixlength n\n"
	}
};
#define	N_IPV6_OPTIONS	(sizeof(ipv6Options) / sizeof(ipv6Options[0]))


static Boolean
set_protocol_ipv6(int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	Boolean	ok;

	ok = _process_options(ipv6Options, N_IPV6_OPTIONS, argc, argv, newConfiguration);
	if (ok) {
		unsigned int	flags;
		CFStringRef	method;
		CFIndex		methodIndex;

		// validate configuration
		method = CFDictionaryGetValue(newConfiguration, kSCPropNetIPv6ConfigMethod);
		methodIndex = _find_selection(method, (selections *)ipv6ConfigMethods, &flags);
		if (methodIndex == kCFNotFound) {
			SCPrint(TRUE, stdout, CFSTR("unknown configuration method\n"));
			return FALSE;
		}

		if (!(flags & allowIPv6Address) && CFDictionaryContainsKey(newConfiguration, kSCPropNetIPv6Addresses)) {
			SCPrint(TRUE, stdout,
				CFSTR("IP address not allowed with %@ configuration\n"),
				ipv6ConfigMethods[methodIndex].selection);
			return FALSE;
		}

		if (!(flags & allowIPv6PrefixLength) && CFDictionaryContainsKey(newConfiguration, kSCPropNetIPv6PrefixLength)) {
			SCPrint(TRUE, stdout,
				CFSTR("Prefix length not allowed with %@ configuration\n"),
				ipv6ConfigMethods[methodIndex].selection);
			return FALSE;
		}

		if (!(flags & allowIPv6Router) && CFDictionaryContainsKey(newConfiguration, kSCPropNetIPv6Router)) {
			SCPrint(TRUE, stdout,
				CFSTR("Router not allowed with %@ configuration\n"),
				ipv6ConfigMethods[methodIndex].selection);
			return FALSE;
		}
	}

	return ok;
}


/* -------------------- */


typedef const struct {
	const char		*proxy;
	const CFStringRef	*keyEnable;
	const CFStringRef	*keyProxy;
	const CFStringRef	*keyPort;
	const CFStringRef	*keyURL;
} proxyKeys;

static proxyKeys proxyKeys_FTP    = { "FTP"   , &kSCPropNetProxiesFTPEnable               , &kSCPropNetProxiesFTPProxy   , &kSCPropNetProxiesFTPPort   , NULL                                       };
static proxyKeys proxyKeys_Gopher = { "Gopher", &kSCPropNetProxiesGopherEnable            , &kSCPropNetProxiesGopherProxy, &kSCPropNetProxiesGopherPort, NULL                                       };
static proxyKeys proxyKeys_HTTP   = { "HTTP"  , &kSCPropNetProxiesHTTPEnable              , &kSCPropNetProxiesHTTPProxy  , &kSCPropNetProxiesHTTPPort  , NULL                                       };
static proxyKeys proxyKeys_HTTPS  = { "HTTPS" , &kSCPropNetProxiesHTTPSEnable             , &kSCPropNetProxiesHTTPSProxy , &kSCPropNetProxiesHTTPSPort , NULL                                       };
static proxyKeys proxyKeys_RTSP   = { "RTSP"  , &kSCPropNetProxiesRTSPEnable              , &kSCPropNetProxiesRTSPProxy  , &kSCPropNetProxiesRTSPPort  , NULL                                       };
static proxyKeys proxyKeys_SOCKS  = { "SOCKS" , &kSCPropNetProxiesSOCKSEnable             , &kSCPropNetProxiesSOCKSProxy , &kSCPropNetProxiesSOCKSPort , NULL                                       };
static proxyKeys proxyKeys_PAC    = { ".pac"  , &kSCPropNetProxiesProxyAutoConfigEnable   , NULL                         , NULL                        , &kSCPropNetProxiesProxyAutoConfigURLString };
static proxyKeys proxyKeys_WPAD   = { "WPAD"  , &kSCPropNetProxiesProxyAutoDiscoveryEnable, NULL                         , NULL                        , NULL                                       };

static proxyKeys	*currentProxy	= NULL;


static int __doProxySelect    (CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration);
static int __doProxyEnable    (CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration);
static int __doProxyHost      (CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration);
static int __doProxyPort      (CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration);
static int __doProxyURL       (CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration);
static int __doProxyFTPPassive(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration);


static options proxyOptions[] = {
	// general options
	{ "ExceptionsList"        , "exceptions", isStringArray, &kSCPropNetProxiesExceptionsList        , NULL              , NULL                      },
	{ "ExcludeSimpleHostnames", NULL        , isBoolean    , &kSCPropNetProxiesExcludeSimpleHostnames, NULL              , NULL                      },
	// proxy selection
	{ "FTP"                   , NULL        , isOther      , NULL                                    , __doProxySelect   , (void *)&proxyKeys_FTP    },
	{ "Gopher"                , NULL        , isOther      , NULL                                    , __doProxySelect   , (void *)&proxyKeys_Gopher },
	{ "HTTP"                  , NULL        , isOther      , NULL                                    , __doProxySelect   , (void *)&proxyKeys_HTTP   },
	{ "HTTPS"                 , NULL        , isOther      , NULL                                    , __doProxySelect   , (void *)&proxyKeys_HTTPS  },
	{ "RTSP"                  , NULL        , isOther      , NULL                                    , __doProxySelect   , (void *)&proxyKeys_RTSP   },
	{ "SOCKS"                 , NULL        , isOther      , NULL                                    , __doProxySelect   , (void *)&proxyKeys_SOCKS  },
	{ "ProxyAutoConfig"       , NULL        , isOther      , NULL                                    , __doProxySelect   , (void *)&proxyKeys_PAC    },
	{   ".pac"                , NULL        , isOther      , NULL                                    , __doProxySelect   , (void *)&proxyKeys_PAC    },
	{ "ProxyAutoDiscovery"    , NULL        , isOther      , NULL                                    , __doProxySelect   , (void *)&proxyKeys_WPAD   },
	{   "WPAD"                , NULL        , isOther      , NULL                                    , __doProxySelect   , (void *)&proxyKeys_WPAD   },
	// proxy modifiers
	{ "disable"               , NULL        , isOther      , NULL                                    , __doProxyEnable    , (void *)FALSE             },
	{ "enable"                , NULL        , isOther      , NULL                                    , __doProxyEnable    , (void *)TRUE              },
	{ "proxy"                 , NULL        , isOther      , NULL                                    , __doProxyHost      , NULL                      },
	{   "host"                , NULL        , isOther      , NULL                                    , __doProxyHost      , NULL                      },
	{ "port"                  , NULL        , isOther      , NULL                                    , __doProxyPort      , NULL                      },
	{ "url"                   , NULL        , isOther      , NULL                                    , __doProxyURL       , NULL                      },
	// (ftp) proxy modifiers
	{ "FTPPassive"            , NULL        , isBoolean    , &kSCPropNetProxiesFTPPassive            , __doProxyFTPPassive, NULL                      },
	{   "passive"             , NULL        , isBoolean    , &kSCPropNetProxiesFTPPassive            , __doProxyFTPPassive, NULL                      },
	// help
	{ "?"                     , NULL        , isHelp       , NULL                                    , NULL                ,
	    "\nProxy configuration commands\n\n"
	    " set protocol ExceptionsList exception[,exception-2]\n"
	    " set protocol ExcludeSimpleHostnames {enable|disable}\n"
	    "\n"
	    " set protocol ftp {enable|disable}\n"
	    " set protocol ftp host proxy-host\n"
	    " set protocol ftp port proxy-port\n"
	    " set protocol ftp passive {enable|disable}\n"
	    "\n"
	    " set protocol http {enable|disable}\n"
	    " set protocol http host proxy-host\n"
	    " set protocol http port proxy-port\n"
	    "\n"
	    " set protocol https {enable|disable}\n"
	    " set protocol https host proxy-host\n"
	    " set protocol https port proxy-port\n"
	    "\n"
	    " set protocol rtsp {enable|disable}\n"
	    " set protocol rtsp host proxy-host\n"
	    " set protocol rtsp port proxy-port\n"
	    "\n"
	    " set protocol socks {enable|disable}\n"
	    " set protocol socks host proxy-host\n"
	    " set protocol socks port proxy-port\n"
	    "\n"
	    " set protocol .pac {enable|disable}\n"
	    " set protocol .pac url .pac-url\n"
	    "\n"
	    " set protocol wpad {enable|disable}\n"
	}
};
#define	N_PROXY_OPTIONS	(sizeof(proxyOptions) / sizeof(proxyOptions[0]))


static int
__doProxySelect(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	CFIndex	nextOption;

	if (argc < 1) {
		SCPrint(TRUE, stdout, CFSTR("proxy option[s] not specified\n"));
		return -1;
	}

	currentProxy = (proxyKeys *)info;

	nextOption = _find_option(argv[0], proxyOptions, N_PROXY_OPTIONS);
	if ((nextOption == kCFNotFound) ||
	    (proxyOptions[nextOption].handler == __doProxySelect)) {
		SCPrint(TRUE, stdout, CFSTR("%s proxy option[s] not specified\n"), currentProxy->proxy);
		return -1;
	}

	return 0;
}


static int
__doProxyEnable(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	Boolean	enabled	= (info == (void *)FALSE) ? FALSE : TRUE;

	if (currentProxy == NULL) {
		SCPrint(TRUE, stdout, CFSTR("proxy not specified\n"));
		return -1;
	}

	if (currentProxy->keyEnable == NULL) {
		SCPrint(TRUE, stdout, CFSTR("%s proxy cannot be %s\n"),
			currentProxy->proxy,
			enabled ? "enabled" : "disabled");
		return -1;
	}


	if (enabled) {
		CFDictionarySetValue(newConfiguration, *(currentProxy->keyEnable), CFNumberRef_1);
	} else {
		CFDictionarySetValue(newConfiguration, *(currentProxy->keyEnable), CFNumberRef_0);

		if (currentProxy->keyProxy != NULL) {
			CFDictionaryRemoveValue(newConfiguration, *(currentProxy->keyProxy));
		}

		if (currentProxy->keyPort != NULL) {
			CFDictionaryRemoveValue(newConfiguration, *(currentProxy->keyPort));
		}

		if (currentProxy->keyURL != NULL) {
			CFDictionaryRemoveValue(newConfiguration, *(currentProxy->keyURL));
		}
	}

	return 0;
}


static Boolean
__proxy_enabled(CFDictionaryRef configuration, const CFStringRef *enableKey)
{
	CFNumberRef	num;
	int		val;

	if (enableKey == NULL) {
		return TRUE;	// if proxy does not need to be enabled
	}

	num = CFDictionaryGetValue(configuration, *enableKey);
	if (!isA_CFNumber(num) ||
	    !CFNumberGetValue(num, kCFNumberIntType, &val) ||
	    (val == 0)) {
		return FALSE;	// if not enabled
	}

	return TRUE;
}


static int
__doProxyHost(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	if (currentProxy == NULL) {
		SCPrint(TRUE, stdout, CFSTR("proxy not specified\n"));
		return -1;
	}

	if (currentProxy->keyProxy == NULL) {
		SCPrint(TRUE, stdout, CFSTR("%s proxy host cannot be specified\n"), currentProxy->proxy);
		return -1;
	}

	if (!__proxy_enabled(newConfiguration, currentProxy->keyEnable)) {
		SCPrint(TRUE, stdout, CFSTR("%s proxy not enabled\n"), currentProxy->proxy);
		return -1;
	}

	if (argc < 1) {
		SCPrint(TRUE, stdout, CFSTR("%s proxy host not specified\n"), currentProxy->proxy);
		return -1;
	}

	if (strlen(argv[0]) > 0) {
		CFStringRef	host;

		host = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
		CFDictionarySetValue(newConfiguration, *(currentProxy->keyProxy), host);
		CFRelease(host);
	} else {
		CFDictionaryRemoveValue(newConfiguration, *(currentProxy->keyProxy));
	}

	return 1;
}


static int
__doProxyPort(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	if (currentProxy == NULL) {
		SCPrint(TRUE, stdout, CFSTR("proxy not specified\n"));
		return -1;
	}

	if (currentProxy->keyPort == NULL) {
		SCPrint(TRUE, stdout, CFSTR("%s proxy port cannot be specified\n"), currentProxy->proxy);
		return -1;
	}

	if (!__proxy_enabled(newConfiguration, currentProxy->keyEnable)) {
		SCPrint(TRUE, stdout, CFSTR("%s proxy not enabled\n"), currentProxy->proxy);
		return -1;
	}

	if (argc < 1) {
		SCPrint(TRUE, stdout, CFSTR("%s proxy port not specified\n"), currentProxy->proxy);
		return -1;
	}

	if (strlen(argv[0]) > 0) {
		CFNumberRef	num;
		int		port;

		num = _copy_number(argv[0]);
		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &port) ||
		    (port < 0) || (port > 65535)) {
			SCPrint(TRUE, stdout, CFSTR("invalid %s proxy port number\n"), currentProxy->proxy);
			return -1;
		}

		CFDictionarySetValue(newConfiguration, *(currentProxy->keyPort), num);
		CFRelease(num);
	} else {
		CFDictionaryRemoveValue(newConfiguration, *(currentProxy->keyPort));
	}

	return 1;
}


static int
__doProxyURL(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	if (currentProxy == NULL) {
		SCPrint(TRUE, stdout, CFSTR("proxy not specified\n"));
		return -1;
	}

	if (currentProxy->keyURL == NULL) {
		SCPrint(TRUE, stdout, CFSTR("%s proxy URL cannot be specified\n"), currentProxy->proxy);
		return -1;
	}

	if (!__proxy_enabled(newConfiguration, currentProxy->keyEnable)) {
		SCPrint(TRUE, stdout, CFSTR("%s proxy not enabled\n"), currentProxy->proxy);
		return -1;
	}

	if (argc < 1) {
		SCPrint(TRUE, stdout, CFSTR("%s proxy URL not specified\n"), currentProxy->proxy);
		return -1;
	}

	if (strlen(argv[0]) > 0) {
		CFStringRef	url;

		url = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
		CFDictionarySetValue(newConfiguration, *(currentProxy->keyURL), url);
		CFRelease(url);
	} else {
		CFDictionaryRemoveValue(newConfiguration, *(currentProxy->keyURL));
	}

	return 1;
}


static int
__doProxyFTPPassive(CFStringRef key, const char *description, void *info, int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	if (currentProxy == NULL) {
		SCPrint(TRUE, stdout, CFSTR("proxy not specified\n"));
		return -1;
	}

	if (currentProxy != &proxyKeys_FTP) {
		SCPrint(TRUE, stdout, CFSTR("passive can only be enable for FTP proxy\n"));
		return -1;
	}

	return 0;
}


static Boolean
set_protocol_proxies(int argc, char **argv, CFMutableDictionaryRef newConfiguration)
{
	Boolean	ok;

	ok = _process_options(proxyOptions, N_PROXY_OPTIONS, argc, argv, newConfiguration);
	return ok;
}


/* -------------------- */


__private_extern__
void
set_protocol(int argc, char **argv)
{
	CFDictionaryRef		configuration;
	CFMutableDictionaryRef	newConfiguration	= NULL;
	Boolean			ok			= FALSE;
	CFStringRef		protocolType;

	if (net_protocol == NULL) {
		SCPrint(TRUE, stdout, CFSTR("protocol not selected\n"));
		return;
	}

	if (argc < 1) {
		SCPrint(TRUE, stdout, CFSTR("set what?\n"));
		return;
	}

	configuration = SCNetworkProtocolGetConfiguration(net_protocol);
	if (configuration == NULL) {
		newConfiguration = CFDictionaryCreateMutable(NULL,
							     0,
							     &kCFTypeDictionaryKeyCallBacks,
							     &kCFTypeDictionaryValueCallBacks);
	} else {
		newConfiguration = CFDictionaryCreateMutableCopy(NULL, 0, configuration);
		CFDictionaryRemoveValue(newConfiguration, kSCResvInactive);
	}

	protocolType = SCNetworkProtocolGetProtocolType(net_protocol);
	if (CFEqual(protocolType, kSCNetworkProtocolTypeAppleTalk)) {
		ok = set_protocol_appletalk(argc, argv, newConfiguration);
	} else if (CFEqual(protocolType, kSCNetworkProtocolTypeDNS)) {
		ok = set_protocol_dns(argc, argv, newConfiguration);
	} else if (CFEqual(protocolType, kSCNetworkProtocolTypeIPv4)) {
		ok = set_protocol_ipv4(argc, argv, newConfiguration);
	} else if (CFEqual(protocolType, kSCNetworkProtocolTypeIPv6)) {
		ok = set_protocol_ipv6(argc, argv, newConfiguration);
	} else if (CFEqual(protocolType, kSCNetworkProtocolTypeProxies)) {
		ok = set_protocol_proxies(argc, argv, newConfiguration);
	} else {
		SCPrint(TRUE, stdout, CFSTR("this protocols configuration cannot be changed\n"));
	}

	if (!ok) {
		goto done;
	}

	if (((configuration == NULL) && (CFDictionaryGetCount(newConfiguration) > 0)) ||
	    ((configuration != NULL) && !CFEqual(configuration, newConfiguration))) {
		if (!SCNetworkProtocolSetConfiguration(net_protocol, newConfiguration)) {
			SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
			goto done;
		}

		net_changed = TRUE;
	}

    done :

	if (newConfiguration != NULL) CFRelease(newConfiguration);
	return;
}


/* -------------------- */


__private_extern__
void
show_protocol(int argc, char **argv)
{
	CFDictionaryRef 	configuration;
	SCNetworkProtocolRef	protocol	= NULL;
	CFStringRef		protocolType;

	if (argc > 0) {
		protocol = _find_protocol(argv[0]);
	} else {
		if (net_protocol != NULL) {
			protocol = net_protocol;
		} else {
			SCPrint(TRUE, stdout, CFSTR("protocol not selected\n"));
			return;
		}
	}

	if (protocol == NULL) {
		return;
	}

	protocolType = SCNetworkProtocolGetProtocolType(protocol);
	SCPrint(TRUE, stdout, CFSTR("protocol type        = %@\n"), protocolType);

	configuration = SCNetworkProtocolGetConfiguration(protocol);
	if (configuration != NULL) {
		SCPrint(TRUE, stdout, CFSTR("\n  protocol configuration\n"));
		_show_entity(configuration, CFSTR(""));
	}

	if (_sc_debug) {
		SCPrint(TRUE, stdout, CFSTR("\n%@\n"), protocol);
	}

	return;
}


/* -------------------- */


__private_extern__
void
show_protocols(int argc, char **argv)
{
	CFIndex	i;
	CFIndex	n;

	if (prefs == NULL) {
		SCPrint(TRUE, stdout, CFSTR("network configuration not open\n"));
		return;
	}

	if (net_service == NULL) {
		SCPrint(TRUE, stdout, CFSTR("service not selected\n"));
		return;
	}

	if (protocols != NULL) CFRelease(protocols);
	protocols = SCNetworkServiceCopyProtocols(net_service);
	if (protocols == NULL) {
		SCPrint(TRUE, stdout, CFSTR("%s\n"), SCErrorString(SCError()));
		return;
	}

	n = CFArrayGetCount(protocols);
	if (n > 1) {
		CFMutableArrayRef	sorted;

		sorted = CFArrayCreateMutableCopy(NULL, 0, protocols);
		CFArraySortValues(sorted,
				  CFRangeMake(0, n),
				  _compare_protocols,
				  NULL);
		CFRelease(protocols);
		protocols = sorted;
	}

	for (i = 0; i < n; i++) {
		SCNetworkProtocolRef	protocol;
		CFStringRef		protocolType;

		protocol     = CFArrayGetValueAtIndex(protocols, i);
		protocolType = SCNetworkProtocolGetProtocolType(protocol);

		SCPrint(TRUE, stdout, CFSTR("%c%2d: %@%*s :"),
			((net_protocol != NULL) && CFEqual(protocol, net_protocol)) ? '>' : ' ',
			i + 1,
			protocolType,
			sizeof("AppleTalk") - CFStringGetLength(protocolType) - 1,
			"");

		if (SCNetworkProtocolGetEnabled(protocol)) {
			CFStringRef	description;

			description = _protocol_description(protocol, FALSE);
			SCPrint(TRUE, stdout, CFSTR(" %@"), description);
			CFRelease(description);
		} else {
			SCPrint(TRUE, stdout, CFSTR(" *DISABLED*"));
		}
		SCPrint(TRUE, stdout, CFSTR("\n"));
	}

	return;
}


/* -------------------- */


__private_extern__
CFStringRef
_protocol_description(SCNetworkProtocolRef protocol, Boolean skipEmpty)
{
	CFDictionaryRef		configuration;
	CFMutableStringRef	description	= NULL;
	CFStringRef		protocolType;

	description = CFStringCreateMutable(NULL, 0);

	if (!SCNetworkProtocolGetEnabled(protocol)) {
		goto done;
	}

	configuration = SCNetworkProtocolGetConfiguration(protocol);
	if (configuration == NULL) {
		goto done;
	}

	protocolType = SCNetworkProtocolGetProtocolType(protocol);
	if (CFEqual(protocolType, kSCNetworkProtocolTypeAppleTalk)) {
		CFStringRef	method;

		method = CFDictionaryGetValue(configuration, kSCPropNetAppleTalkConfigMethod);
		if (isA_CFString(method)) {
			CFStringAppendFormat(description,
					     NULL,
					     CFSTR("%@"),
					     method);
		}
	} else if (CFEqual(protocolType, kSCNetworkProtocolTypeDNS)) {
		CFStringRef	domain;
		CFArrayRef	search;
		CFArrayRef	servers;

		domain  = CFDictionaryGetValue(configuration, kSCPropNetDNSDomainName);
		if (isA_CFString(domain)) {
			CFStringAppendFormat(description,
					     NULL,
					     CFSTR("domain=%@"),
					     domain);
		}

		search = CFDictionaryGetValue(configuration, kSCPropNetDNSSearchDomains);
		if (isA_CFArray(search)) {
			CFStringRef	str;

			str = CFStringCreateByCombiningStrings(NULL, search, CFSTR(","));
			CFStringAppendFormat(description,
					     NULL,
					     CFSTR("%ssearch=%@"),
					     CFStringGetLength(description) > 0 ? ", " : "",
					     str);
			CFRelease(str);
		}

		servers = CFDictionaryGetValue(configuration, kSCPropNetDNSServerAddresses);
		if (isA_CFArray(servers)) {
			CFStringRef	str;

			str = CFStringCreateByCombiningStrings(NULL, servers, CFSTR(","));
			CFStringAppendFormat(description,
					     NULL,
					     CFSTR("%sservers=%@"),
					     CFStringGetLength(description) > 0 ? ", " : "",
					     str);
			CFRelease(str);
		}
	} else if (CFEqual(protocolType, kSCNetworkProtocolTypeIPv4)) {
		CFStringRef	method;

		method = CFDictionaryGetValue(configuration, kSCPropNetIPv4ConfigMethod);
		if (isA_CFString(method)) {
			CFArrayRef	addresses;

			addresses = CFDictionaryGetValue(configuration, kSCPropNetIPv4Addresses);
			if (CFEqual(method, kSCValNetIPv4ConfigMethodINFORM) &&
			    isA_CFArray(addresses)) {
				CFStringAppendFormat(description,
						     NULL,
						     CFSTR("%@, address=%@"),
						     method,
						     CFArrayGetValueAtIndex(addresses, 0));
			} else if (CFEqual(method, kSCValNetIPv4ConfigMethodManual) &&
				   isA_CFArray(addresses)) {
				CFStringAppendFormat(description,
						     NULL,
						     CFSTR("%@, address=%@"),
						     method,
						     CFArrayGetValueAtIndex(addresses, 0));
			} else {
				CFStringAppendFormat(description,
						     NULL,
						     CFSTR("%@"),
						     method);
			}
		}
	} else if (CFEqual(protocolType, kSCNetworkProtocolTypeIPv6)) {
		CFStringRef	method;

		method = CFDictionaryGetValue(configuration, kSCPropNetIPv6ConfigMethod);
		if (isA_CFString(method)) {
			CFStringAppendFormat(description,
					     NULL,
					     CFSTR("%@"),
					     method);
		}
	} else if (CFEqual(protocolType, kSCNetworkProtocolTypeProxies)) {
		CFIndex			i;
		static proxyKeys	*keys[] = { &proxyKeys_FTP,	&proxyKeys_Gopher,	&proxyKeys_HTTP,	&proxyKeys_HTTPS,
						    &proxyKeys_RTSP,	&proxyKeys_SOCKS,	&proxyKeys_PAC,		&proxyKeys_WPAD	};

		for (i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
			proxyKeys	*currentProxy	= keys[i];

			if (!__proxy_enabled(configuration, currentProxy->keyEnable)) {
				continue;
			}

			if (((currentProxy->keyProxy != NULL) &&
			     !CFDictionaryContainsKey(configuration, *(currentProxy->keyProxy))) ||
			    ((currentProxy->keyURL != NULL) &&
			     !CFDictionaryContainsKey(configuration, *(currentProxy->keyURL)))) {
				continue;
			}

			CFStringAppendFormat(description,
					     NULL,
					     CFSTR("%s%s"),
					     CFStringGetLength(description) > 0 ? ", " : "",
					     currentProxy->proxy);
		}
	}

    done :

	if (skipEmpty && CFStringGetLength(description) == 0) {
		CFRelease(description);
		description = NULL;
	}

	return description;
}
