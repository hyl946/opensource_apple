#
# Selectable features for MacOSX
#

# Legacy *64 APIs
FEATURE_LEGACY_64_APIS = 1

# Legacy NX international APIs
FEATURE_LEGACY_NX_INTERNAT_APIS = 1

# Legacy NXZone APIs
FEATURE_LEGACY_NXZONE_APIS = 1

# Legacy rune APIs
FEATURE_LEGACY_RUNE_APIS = 1

# Legacy utmp APIs
FEATURE_LEGACY_UTMP_APIS = 1

# New OSMemoryNotification and OSThermalNotification APIs
#FEATURE_MEM_THERM_NOTIFICATION_APIS = 1

# Move localtime to /var/db/timezone
#FEATURE_MOVE_LOCALTIME = 1

# Long doubles are doubles (should match sys/cdefs.h)
.if (${MACHINE_ARCH} == arm)
FEATURE_ONLY_LONG_DOUBLE_IS_DOUBLE = 1
.endif

# No pre-1050 variants (should match sys/cdefs.h)
#FEATURE_ONLY_1050_VARIANTS = 1

# No legacy variants (should match sys/cdefs.h)
.if defined(LP64)
FEATURE_ONLY_UNIX_CONFORMANCE = 1
.endif

# Only 64-bit ino_t (should match sys/cdefs.h)
#FEATURE_ONLY_64_BIT_INO_T = 1

# Patch 3333969
FEATURE_PATCH_3333969 = 1

# Patch 3375657
FEATURE_PATCH_3375657 = 1

# Patch 3417676
FEATURE_PATCH_3417676 = 1

# Patch 5243343
FEATURE_PATCH_5243343 = 1

# plockstat dtrace support
FEATURE_PLOCKSTAT = 1

# Timezone change notification
FEATURE_TIMEZONE_CHANGE_NOTIFICATION = 1
