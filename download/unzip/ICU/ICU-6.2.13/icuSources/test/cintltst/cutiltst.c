/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2004, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File CUTILTST.C
*
* Modification History:
*        Name                     Description
*     Madhu Katragadda               Creation
*********************************************************************************
*/
#include "cintltst.h"

void addLocaleTest(TestNode**);
void addCLDRTest(TestNode**);
void addUnicodeTest(TestNode**);
void addUStringTest(TestNode**);
void addResourceBundleTest(TestNode**);
void addNEWResourceBundleTest(TestNode**);
void addHashtableTest(TestNode** root);
void addCStringTest(TestNode** root);
void addTrieTest(TestNode** root);
void addEnumerationTest(TestNode** root);
void addPosixTest(TestNode** root);
void addSortTest(TestNode** root);

void addUtility(TestNode** root);

void addUtility(TestNode** root)
{
    addCStringTest(root);
    addTrieTest(root);
    addLocaleTest(root);
    addCLDRTest(root);
    addUnicodeTest(root);
    addUStringTest(root);
    addResourceBundleTest(root);
    addNEWResourceBundleTest(root);
    addHashtableTest(root);
    addEnumerationTest(root);
    addPosixTest(root);
    addSortTest(root);
}
