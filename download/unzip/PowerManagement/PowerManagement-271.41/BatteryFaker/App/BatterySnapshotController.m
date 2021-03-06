/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
#import <assert.h>

#import "BatterySnapshotController.h"
static NSString   *kSnapFileName = @"BatterySnapshots.xml";

@implementation BatterySnapshotController

- (id)init
{
    NSArray     *fileContents = NULL;
    NSString    *snapFilePath = NULL;
    
    int             i = 0;
    NSObject        *obj;
    NSString        *insertString = NULL;
    NSDictionary    *snapshotDictionary = NULL;
    
    
    orderedMenuTitles = [[NSMutableArray alloc] init];
    snapshotDescriptions = [[NSMutableDictionary alloc] init];
    
    /** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **
     ** Open file and read array
     ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/
    snapFilePath = [[NSBundle mainBundle] resourcePath];
    snapFilePath = [snapFilePath stringByAppendingPathComponent:kSnapFileName];

    fileContents = [NSArray arrayWithContentsOfFile:snapFilePath];
    assert(fileContents);

    /** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **
     ** Build menu titles array
     ** And dictionary mapping titles to dictionaries.
     ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** **/
    for (i=0; i<[fileContents count]; i++) 
    {
        insertString = NULL;
        obj = [fileContents objectAtIndex:i];
        if ([obj isKindOfClass:[NSString class]])
        {
            insertString = (NSString *)obj;
            assert( [insertString isEqualToString:@"Menu Separator"]);
        } else if ([obj isKindOfClass:[NSDictionary class]])
        {
            snapshotDictionary = (NSDictionary *)obj;
            insertString = [snapshotDictionary objectForKey:@"SnapshotTitle"];
        
            [snapshotDescriptions setObject:snapshotDictionary
                                   forKey:insertString];
        }

        if (insertString) {
            [orderedMenuTitles addObject:insertString];
        }
    }

    return [super init];
}

- (void) free
{
    [orderedMenuTitles release];
    [snapshotDescriptions release];
}

- (NSArray *)menuTitlesForSnapshots
{
    return orderedMenuTitles;
}

- (NSDictionary *)batterySnapshotForTitle:(NSString *)title
{
    NSLog(@"%@", [[snapshotDescriptions objectForKey:title] description]);
    return [snapshotDescriptions objectForKey:title];
}

@end