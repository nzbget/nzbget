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

@protocol DaemonControllerDelegate

- (void)daemonConfigLoaded;
- (void)daemonStatusUpdated;

@end


@interface DaemonController : NSObject {
	NSString* lockFilePath;
	NSDictionary* config;
	int restartCounter;
	int restartPid;
	NSTimer* updateTimer;
	int lastUptime;
	BOOL factoryReset;
}

@property (nonatomic, assign) NSTimeInterval updateInterval;
@property (nonatomic, readonly) BOOL connected;
@property (nonatomic, readonly) BOOL restarting;
@property (nonatomic, readonly) BOOL recoveryMode;
@property (nonatomic, readonly) NSString* configFilePath;
@property (nonatomic, readonly) NSString* browserUrl;
@property (nonatomic, readonly) NSDate* lastUpdate;
@property (nonatomic, assign) id<DaemonControllerDelegate> delegate;
@property (nonatomic, readonly) NSDictionary* status;

- (id)init;

- (NSString*)valueForOption:(NSString*)option;

- (void)start;

- (void)stop;

- (void)restartInRecoveryMode:(BOOL)recovery withFactoryReset:(BOOL)reset;

- (NSString *)browserUrl;

- (void)updateStatus;

- (void)rpc:(NSString*)method success:(SEL)successCallback failure:(SEL)failureCallback;

- (void)setUpdateInterval:(NSTimeInterval)updateInterval;

@end
