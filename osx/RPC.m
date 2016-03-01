/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#import <Cocoa/Cocoa.h>
#import "RPC.h"

@implementation RPC

NSString* rpcUrl;

- (id)initWithMethod:(NSString*)method
			receiver:(id)receiver
			 success:(SEL)successCallback
			 failure:(SEL)failureCallback {
	NSString* urlStr = [rpcUrl stringByAppendingString:method];
	self = [super initWithURLString:urlStr receiver:receiver success:successCallback failure:failureCallback];
	return self;
}

+ (void)setRpcUrl:(NSString*)url {
	rpcUrl = url;
}

- (void)success {
    NSError *error = nil;
    id dataObj = [NSJSONSerialization
				  JSONObjectWithData:data
				  options:0
				  error:&error];
	
    if (error || ![dataObj isKindOfClass:[NSDictionary class]]) {
		/* JSON was malformed, act appropriately here */
		failureCode = 999;
		[self failure];
	}

	id result = [dataObj valueForKey:@"result"];
	SuppressPerformSelectorLeakWarning([_receiver performSelector:_successCallback withObject:result];);
}

@end
