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

#include <libproc.h>
#import <Cocoa/Cocoa.h>
#import "DaemonController.h"
#import "RPC.h"

NSString* MAIN_DIR = @"${AppSupDir}";
NSString* LOCK_FILE = @"${AppSupDir}/nzbget.lock";
NSString* CONFIG_FILE = @"${AppSupDir}/nzbget.conf";
NSString* SCRIPTS_DIR = @"${AppSupDir}/scripts";
NSString* NZB_DIR = @"${AppSupDir}/nzb";
NSString* QUEUE_DIR = @"${AppSupDir}/queue";
NSString* TMP_DIR = @"${AppSupDir}/tmp";

@implementation DaemonController

- (id)init {
	self = [super init];
	
	_configFilePath = [self resolveAppSupDir:CONFIG_FILE];
	lockFilePath = [self resolveAppSupDir:LOCK_FILE];
	
	return self;
}

- (NSString *) bundlePath {
	return [[NSBundle mainBundle] pathForResource:@"daemon" ofType:nil];
}

- (NSString *) resolveAppSupDir:(NSString *)dir {
    NSString *appSupPath = [@"${AppSupDir}/Application Support/NZBGet" stringByExpandingTildeInPath];

	NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    if (paths.count > 0)
    {
		appSupPath = [paths objectAtIndex:0];
		appSupPath = [appSupPath stringByAppendingPathComponent:@"NZBGet"];
    }
	
	dir = [dir stringByReplacingOccurrencesOfString:@"${AppSupDir}"
										 withString:appSupPath
											options:NSCaseInsensitiveSearch
											  range:NSMakeRange(0, dir.length)];
	return dir;
}

- (void) checkDefaults {
	NSString* mainDir = [self resolveAppSupDir:MAIN_DIR];
	if (![[NSFileManager defaultManager] fileExistsAtPath:mainDir]) {
		[[NSFileManager defaultManager] createDirectoryAtPath:mainDir withIntermediateDirectories:YES attributes:nil error:nil];
	}
	
	NSString* bundlePath = [self bundlePath];

	if (![[NSFileManager defaultManager] fileExistsAtPath:_configFilePath]) {
		NSString* configTemplate = [NSString stringWithFormat:@"%@/usr/local/share/nzbget/nzbget.conf", bundlePath];
		[[NSFileManager defaultManager] copyItemAtPath:configTemplate toPath:_configFilePath error:nil];
	}

	NSString* scriptsDir = [self resolveAppSupDir:SCRIPTS_DIR];
	if (![[NSFileManager defaultManager] fileExistsAtPath:scriptsDir]) {
		NSString* scriptsTemplate = [NSString stringWithFormat:@"%@/usr/local/share/nzbget/scripts", bundlePath];
		[[NSFileManager defaultManager] copyItemAtPath:scriptsTemplate toPath:scriptsDir error:nil];
	}
}

- (int)readLockFilePid {
	if ([[NSFileManager defaultManager] fileExistsAtPath:lockFilePath]) {
		// Lock file exists
		// read pid from lock file
		int pid = [[NSString stringWithContentsOfFile:lockFilePath encoding:NSUTF8StringEncoding error:nil] intValue];
		DLog(@"pid: %i", pid);
		
		// check if the process name is "nzbget" to avoid killing of other proceses
		char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
		int ret = proc_pidpath(pid, pathbuf, sizeof(pathbuf));
		if (ret <= 0) {
			// error
			return 0;
		}
		
		DLog(@"proc %d: %s\n", pid, pathbuf);
		
		NSString* instancePath = [NSString stringWithUTF8String:pathbuf];
		if ([instancePath hasSuffix:@".app/Contents/Resources/daemon/usr/local/bin/nzbget"]) {
			return pid;
		}
	}
	
	return 0;
}

- (void)killDaemonWithSignal:(int)signal {
	int pid = [self readLockFilePid];
	if (pid > 0) {
		kill(pid, signal);
		[[NSFileManager defaultManager] removeItemAtPath:lockFilePath error:nil];
	}
}

- (void)start {
	DLog(@"DaemonController->start");
	
	[self checkDefaults];
	[self killDaemonWithSignal:SIGKILL];
	[self readConfigFile];
	[self initRpcUrl];
	[self initBrowserUrl];
	
	NSString* bundlePath = [self bundlePath];
	NSString* daemonPath = [NSString stringWithFormat:@"%@/usr/local/bin/nzbget", bundlePath];
	NSString* optionWebDir = [NSString stringWithFormat:@"WebDir=%@/usr/local/share/nzbget/webui", bundlePath];
	NSString* optionConfigTemplate = [NSString stringWithFormat:@"ConfigTemplate=%@/usr/local/share/nzbget/nzbget.conf", bundlePath];
	NSString* optionLockFile = [NSString stringWithFormat:@"LockFile=%@", lockFilePath];
	
	NSMutableArray* arguments = [NSMutableArray arrayWithObjects:
		@"-c", _configFilePath,
		@"-D",
		@"-o", optionWebDir,
		@"-o", optionConfigTemplate,
		@"-o", optionLockFile,
		nil];

	if (_recoveryMode) {
		[arguments addObjectsFromArray: [NSArray arrayWithObjects:
			@"-o", @"ControlIP=127.0.0.1",
			@"-o", @"ControlPort=6789",
			@"-o", @"ControlPassword=",
			@"-o", @"SecureControl=no",
			nil
		]];
	}

	NSTask* task = [[NSTask alloc] init];
	[task setLaunchPath: daemonPath];
	[task setArguments: arguments];
	[task launch];
	
	_restarting = NO;
	[self scheduleNextUpdate];
}

- (void)stop {
	DLog(@"DaemonController->stop");
	[self killDaemonWithSignal:SIGTERM];
}

- (void)restartInRecoveryMode:(BOOL)recovery withFactoryReset:(BOOL)reset {
	_recoveryMode = recovery;
	factoryReset = reset;
	_restarting = YES;
	restartPid = [self readLockFilePid];
	[self stop];

	// in timer wait for deletion of lockfile for 10 seconds,
	// after that call "start" which will kill the old process.
	restartCounter = 0;
	[self restartWait];
}

- (void)restartWait {
	DLog(@"DaemonController->restartWait");
	restartCounter++;
	
	char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
	int ret = proc_pidpath(restartPid, pathbuf, sizeof(pathbuf));
	if (ret > 0 && restartCounter < 100) {
		DLog(@"restartWait: scheduleNextRestartWait");
		[self scheduleNextRestartWait];
	} else {
		DLog(@"restartWait: start");
		if (factoryReset) {
			[self resetToFactoryDefaults];
		}
		[self start];
	}
}

- (void)resetToFactoryDefaults {
	DLog(@"DaemonController->resetToFactoryDefaults");
	[[NSFileManager defaultManager] removeItemAtPath:_configFilePath error:nil];
	[[NSFileManager defaultManager] removeItemAtPath:[self resolveAppSupDir:QUEUE_DIR] error:nil];
	[[NSFileManager defaultManager] removeItemAtPath:[self resolveAppSupDir:SCRIPTS_DIR] error:nil];
	[[NSFileManager defaultManager] removeItemAtPath:[self resolveAppSupDir:NZB_DIR] error:nil];
	[[NSFileManager defaultManager] removeItemAtPath:[self resolveAppSupDir:TMP_DIR] error:nil];
}

- (void)scheduleNextRestartWait {
	NSTimer* timer = [NSTimer timerWithTimeInterval:0.100 target:self selector:@selector(restartWait) userInfo:nil repeats:NO];
	[[NSRunLoop currentRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
}

- (void)readConfigFile {
	DLog(@"DaemonController->readConfigFile");
	NSString* str = [NSString stringWithContentsOfFile:_configFilePath encoding:NSUTF8StringEncoding error:nil];
	NSArray* conf = [str componentsSeparatedByString: @"\n"];
	config = [[NSMutableDictionary alloc] init];

	for (NSString* opt in conf) {
		if ([opt hasPrefix:@"#"]) {
			continue;
		}
		NSRange pos = [opt rangeOfString:@"="];
		if (pos.location != NSNotFound) {
			NSString* name = [opt substringToIndex:pos.location];
			name = [name stringByTrimmingCharactersInSet: [NSCharacterSet whitespaceCharacterSet]];
			NSString* value = [opt substringFromIndex:pos.location + 1];
			value = [value stringByTrimmingCharactersInSet: [NSCharacterSet whitespaceCharacterSet]];
			[config setValue:value forKey:[name uppercaseString]];
		}
	}

	if (_recoveryMode) {
		[config setValue:@"localhost" forKey:@"CONTROLIP"];
		[config setValue:@"6789" forKey:@"CONTROLPORT"];
		[config setValue:@"" forKey:@"CONTROLPASSWORD"];
	}

	[_delegate daemonConfigLoaded];
}

- (NSString*)valueForOption:(NSString*)option {
	return [config valueForKey:[option uppercaseString]];
}

- (void)initBrowserUrl {
	NSString* ip = [self valueForOption:@"ControlIP"];
	if ([ip isEqualToString:@"0.0.0.0"] || [ip isEqualToString:@"127.0.0.1"]) {
		ip = @"localhost";
	}
	NSString* port = [self valueForOption:@"ControlPort"];
	_browserUrl = [NSString stringWithFormat:@"http://@%@:%@", ip, port];
}

-(NSString *)urlEncode:(NSString*)str {
	return (NSString *)CFBridgingRelease(CFURLCreateStringByAddingPercentEscapes(NULL,
		(CFStringRef)str,
		NULL,
		(CFStringRef)@"!*'\"();:@&=+$,/?%#[]% ",
		kCFStringEncodingUTF8));
}

- (void)initRpcUrl {
	NSString* ip = [self valueForOption:@"ControlIP"];
	if ([ip isEqualToString:@"0.0.0.0"]) {
		ip = @"127.0.0.1";
	}
	NSString* port = [self valueForOption:@"ControlPort"];
	NSString* username = [self urlEncode:[self valueForOption:@"ControlUsername"]];
	NSString* password = [self urlEncode:[self valueForOption:@"ControlPassword"]];
	NSString* RpcUrl = [NSString stringWithFormat:@"http://%@:%@/%@:%@/jsonrpc/", ip, port, username, password];
	[RPC setRpcUrl:RpcUrl];
}

- (void)setUpdateInterval:(NSTimeInterval)updateInterval {
	_updateInterval = updateInterval;
	if (_connected) {
		[updateTimer invalidate];
		[self updateStatus];
	}
}

- (void)scheduleNextUpdate {
	updateTimer = [NSTimer timerWithTimeInterval:_updateInterval target:self selector:@selector(updateStatus) userInfo:nil repeats:NO];
	[[NSRunLoop currentRunLoop] addTimer:updateTimer forMode:NSRunLoopCommonModes];
}

- (void)rpc:(NSString*)method success:(SEL)successCallback failure:(SEL)failureCallback {
	RPC* rpc = [[RPC alloc] initWithMethod:method receiver:self success:successCallback failure:failureCallback];
	[rpc start];
}

- (void)receiveStatus:(NSDictionary*)status {
	//DLog(@"receiveStatus");
	if (_restarting) {
		return;
	}

	_connected = YES;
	_lastUpdate = [NSDate date];
	_status = status;

	//DLog(@"response: %@", status);
	
	int uptime = [(NSNumber*)[status objectForKey:@"UpTimeSec"] integerValue];
	if (lastUptime == 0) {
		lastUptime = uptime;
	} else if (lastUptime > uptime) {
		// daemon was reloaded (soft-restart)
		[self readConfigFile];
	}

	[_delegate daemonStatusUpdated];
	[self scheduleNextUpdate];
}

- (void)failureStatus {
	DLog(@"failureStatus");
	if (_restarting) {
		return;
	}

	_connected = NO;

	int pid = [self readLockFilePid];
	if (pid == 0) {
		// Daemon is not running. Crashed?
		_restarting = YES;
		[_delegate daemonStatusUpdated];
		[self start];
	} else {
		[_delegate daemonStatusUpdated];
		[self scheduleNextUpdate];
	}
}

- (void)updateStatus {
	if (_restarting) {
		return;
	}
	[self rpc:@"status" success:@selector(receiveStatus:) failure:@selector(failureStatus)];
}

@end
