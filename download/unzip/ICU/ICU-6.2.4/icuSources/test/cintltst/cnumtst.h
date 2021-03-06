/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2004, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File CNUMTST.H
*
* Modification History:
*        Name                     Description            
*     Madhu Katragadda              Creation
*********************************************************************************
*/
/* C API TEST FOR NUMBER FORMAT */
#ifndef _CNUMFRMTST
#define _CNUMFRMTST

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "cintltst.h"


/**
 * The function used to test the Number format API
 **/
static void TestNumberFormat(void);

/**
 * The function used to test significant digits in the Number format API
 **/
static void TestSignificantDigits(void);

/**
 * The function used to test the Number format API with padding
 **/
static void TestNumberFormatPadding(void);

/**
 * The function used to test the Number format API with padding
 **/
static void TestInt64Format(void);

static void TestNonExistentCurrency(void);

/**
 * Test RBNF access through unumfmt APIs.
 **/
static void TestRBNFFormat(void);

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif
