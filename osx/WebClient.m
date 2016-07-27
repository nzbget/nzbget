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
#import "WebClient.h"

@implementation WebClient

- (id)initWithURLString:(NSString*)urlStr
			   receiver:(id)receiver
				success:(SEL)successCallback
				failure:(SEL)failureCallback {
	self = [super init];
	
	_receiver = receiver;
	_successCallback = successCallback;
	_failureCallback = failureCallback;
	NSURL *url = [NSURL URLWithString:urlStr];
	NSURLRequest *request = [NSURLRequest requestWithURL:url];
	connection = [[NSURLConnection alloc] initWithRequest:request delegate:self startImmediately:NO];
	[connection scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];

	return self;
}

- (void) start {
	[connection start];
}

- (void)connection:(NSURLConnection *)aConnection didReceiveResponse:(NSHTTPURLResponse *)aResponse {
	responseHeaderFields = [aResponse allHeaderFields];
	
	if ([aResponse statusCode] != 200)
	{
		failureCode = [aResponse statusCode];
		[connection cancel];
		[self failure];
		return;
	}
	
	NSInteger contentLength = [[responseHeaderFields objectForKey:@"Content-Length"] integerValue];
	if (contentLength > 0) {
		data = [[NSMutableData alloc] initWithCapacity:contentLength];
	} else {
		data = [[NSMutableData alloc] init];
	}
}

- (void)connection:(NSURLConnection *)aConnection didReceiveData:(NSData *)newData {
	[data appendData:newData];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)aConnection {
	[connection cancel];
	[self success];
}

- (void)connection:(NSURLConnection *)aConnection didFailWithError:(NSError *)error {
	if ([[error domain] isEqual:NSURLErrorDomain])
	{
		failureCode = [error code];
	}
	
	[connection cancel];
	[self failure];
}

- (void)success {
	SuppressPerformSelectorLeakWarning([_receiver performSelector:_successCallback withObject:data];);
}

- (void)failure {
	SuppressPerformSelectorLeakWarning([_receiver performSelector:_failureCallback];);
}

@end
