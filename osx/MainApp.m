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

#import "MainApp.h"
#import "PreferencesDialog.h"
#import "WelcomeDialog.h"
#import "PFMoveApplication.h"

NSString *PreferencesContext = @"PreferencesContext";
const NSTimeInterval NORMAL_UPDATE_INTERVAL = 10.000;
const NSTimeInterval MENUOPEN_UPDATE_INTERVAL = 1.000;
const NSTimeInterval START_UPDATE_INTERVAL = 0.500;

int main(int argc, char *argv[]) {
	return NSApplicationMain(argc, (const char **)argv);
}

/*
 * Signal handler
 */
void SignalProc(int iSignal)
{
	switch (iSignal)
	{
		case SIGINT:
		case SIGTERM:
			signal(iSignal, SIG_DFL);   // Reset the signal handler
			[NSApp terminate:nil];
			break;
	}
}

// we install seignal handler in order to properly terminat app from Activity Mo1nitor
void InstallSignalHandlers()
{
	signal(SIGINT, SignalProc);
	signal(SIGTERM, SignalProc);
	signal(SIGPIPE, SIG_IGN);
}

@implementation MainApp

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification {
	[self checkOtherRunningInstances];
#ifndef DEBUG
	PFMoveToApplicationsFolderIfNecessary();
#endif
}

- (void)checkOtherRunningInstances {
	for (NSRunningApplication *runningApplication in [NSRunningApplication runningApplicationsWithBundleIdentifier:[[NSBundle mainBundle] bundleIdentifier]]) {
		if (![[runningApplication executableURL] isEqualTo:[[NSRunningApplication currentApplication] executableURL]]) {
			NSString *executablePath = [[runningApplication executableURL] path];
			executablePath = [[[executablePath stringByDeletingLastPathComponent] stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];
			DLog(@"Switching to an already running instance: %@", executablePath);
			[[NSTask launchedTaskWithLaunchPath:@"/usr/bin/open"
									  arguments:[NSArray arrayWithObjects:executablePath, @"--args", @"--second-instance", nil]] waitUntilExit];
			exit(0);
		}
	}
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	BOOL autoStartWebUI = [[NSUserDefaults standardUserDefaults] boolForKey:@"AutoStartWebUI"];

	daemonController = [[DaemonController alloc] init];
	daemonController.updateInterval = autoStartWebUI ? START_UPDATE_INTERVAL : NORMAL_UPDATE_INTERVAL;
	daemonController.delegate = self;

	[self setupDefaultsObserver];
	[self userDefaultsDidChange:nil];
	
    if (![MainApp wasLaunchedAsLoginItem]) {
        [self showWelcomeScreen];
    }

    InstallSignalHandlers();

	DLog(@"Start Daemon");
	[daemonController start];
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)flag {
	DLog(@"applicationShouldHandleReopen");
	[self showAlreadyRunning];
	return YES;
}

+ (void)initialize {
	[self setupAppDefaults];
}

- (void)awakeFromNib {
	DLog(@"awakeFromNib");
	[statusMenu setDelegate:self];
}

+ (void)setupAppDefaults {
	NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
	NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
								 @"YES", @"ShowInMenubar",
								 @"NO", @"Autostart",
								 @"YES", @"AutoStartWebUI",
								 nil];
	[userDefaults registerDefaults:appDefaults];
}

- (void)setupDefaultsObserver {
	NSUserDefaultsController *sdc = [NSUserDefaultsController sharedUserDefaultsController];
	[sdc addObserver:self forKeyPath:@"values.ShowInMenubar" options:0 context:(__bridge void *)(PreferencesContext)];
	[sdc addObserver:self forKeyPath:@"values.AutoStartWebUI" options:0 context:(__bridge void *)(PreferencesContext)];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context {
	if (context == (__bridge void *)(PreferencesContext)) {
		[self userDefaultsDidChange:nil];
	}
	else {
		[super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
	}
}

- (void)userDefaultsDidChange:(id)sender {
	DLog(@"userDefaultsDidChange: %@", sender);
	BOOL showInMenubar = [[NSUserDefaults standardUserDefaults] boolForKey:@"ShowInMenubar"];
	if (showInMenubar != (statusItem != nil)) {
		if (showInMenubar) {
			statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
			[statusItem setHighlightMode:YES];
			[statusItem setMenu:statusMenu];
			NSImage* icon = [NSImage imageNamed:@"statusicon.png"];
			[icon setTemplate:YES];
			[statusItem setImage:icon];
		}
		else {
			statusItem = nil;
		}
	}
}

- (IBAction)preferencesClicked:(id)sender {
	[self showPreferences];
}

- (void)showPreferences {
	[NSApp activateIgnoringOtherApps:TRUE];
	if (preferencesDialog) {
		return;
	}
	preferencesDialog = [[PreferencesDialog alloc] init];
	[[preferencesDialog window] center];
	[preferencesDialog showWindow:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(preferencesWillClose:) 
												 name:NSWindowWillCloseNotification
											   object:[preferencesDialog window]];
}

- (void)preferencesWillClose:(NSNotification *)notification {
    DLog(@"Pref Closed");
    [[NSNotificationCenter defaultCenter] removeObserver:self
													name:NSWindowWillCloseNotification 
												  object:[preferencesDialog window]];
	preferencesDialog = nil;
	[self userDefaultsDidChange:nil];
}

- (IBAction)aboutClicked:(id)sender {
	[NSApp activateIgnoringOtherApps:TRUE];
	[NSApp orderFrontStandardAboutPanel:nil];
}

- (IBAction)quitClicked:(id)sender {
	DLog(@"Quit");	
	[NSApp terminate:nil];
}

- (void)showWelcomeScreen {
    welcomeDialog = [[WelcomeDialog alloc] init];
    [(WelcomeDialog*)welcomeDialog setMainDelegate:self];
    [(WelcomeDialog*)welcomeDialog showDialog];
}

- (void)showAlreadyRunning {
    BOOL showInMenubar = [[NSUserDefaults standardUserDefaults] boolForKey:@"ShowInMenubar"];
	
	[NSApp activateIgnoringOtherApps:TRUE];
	NSAlert *alert = [[NSAlert alloc] init];
	[alert setMessageText:NSLocalizedString(@"AlreadyRunning.MessageText", nil)];
	[alert setInformativeText:NSLocalizedString(showInMenubar ? @"AlreadyRunning.InformativeTextWithIcon" : @"AlreadyRunning.InformativeTextWithoutIcon", nil)];
	NSButton* webUIButton = [alert addButtonWithTitle:NSLocalizedString(@"AlreadyRunning.WebUI", nil)];
	[alert addButtonWithTitle:NSLocalizedString(@"AlreadyRunning.Preferences", nil)];
	[alert addButtonWithTitle:NSLocalizedString(@"AlreadyRunning.Quit", nil)];
	[alert.window makeFirstResponder:webUIButton];
	[webUIButton setKeyEquivalent:@"\r"];
	switch ([alert runModal]) {
		case NSAlertFirstButtonReturn:
			[self showWebUI];
			break;
			
		case NSAlertSecondButtonReturn:
			[self showPreferences];
			break;
			
		case NSAlertThirdButtonReturn:
			[self quitClicked:nil];
			break;
	}
}

+ (BOOL)wasLaunchedByProcess:(NSString*)creator {
    BOOL  wasLaunchedByProcess = NO;
    
    // Get our PSN
    OSStatus  err;
    ProcessSerialNumber currPSN;
    err = GetCurrentProcess (&currPSN);
    if (!err) {
        // We don't use ProcessInformationCopyDictionary() because the 'ParentPSN' item in the dictionary
        // has endianness problems in 10.4, fixed in 10.5 however.
        ProcessInfoRec  procInfo;
        bzero (&procInfo, sizeof (procInfo));
        procInfo.processInfoLength = (UInt32)sizeof (ProcessInfoRec);
        err = GetProcessInformation (&currPSN, &procInfo);
        if (!err) {
            ProcessSerialNumber parentPSN = procInfo.processLauncher;
            
            // Get info on the launching process
            NSDictionary* parentDict = (__bridge NSDictionary*)ProcessInformationCopyDictionary (&parentPSN, kProcessDictionaryIncludeAllInformationMask);
            
            // Test the creator code of the launching app
            if (parentDict) {
                wasLaunchedByProcess = [[parentDict objectForKey:@"FileCreator"] isEqualToString:creator];
            }
        }
    }
    
    return wasLaunchedByProcess;
}

+ (BOOL)wasLaunchedAsLoginItem {
    // If the launching process was 'loginwindow', we were launched as a login item
    return [self wasLaunchedByProcess:@"lgnw"];
}

- (IBAction)webuiClicked:(id)sender {
	if (daemonController.connected) {
		[self showWebUI];
	} else {
		[NSApp activateIgnoringOtherApps:TRUE];
		NSAlert *alert = [[NSAlert alloc] init];
		[alert setMessageText:NSLocalizedString(@"ShowWebUINoConnection.MessageText", nil)];
		[alert setInformativeText:NSLocalizedString(@"ShowWebUINoConnection.InformativeText", nil)];
		[alert setAlertStyle:NSWarningAlertStyle];
		[alert runModal];
	}
}

- (void)showWebUI {
	DLog(@"showWebUI");
	[[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString:daemonController.browserUrl]];
}

- (IBAction)openConfigInTextEditClicked:(id)sender {
	NSString *configFile = [daemonController configFilePath];
	[[NSWorkspace sharedWorkspace] openFile:configFile withApplication:@"TextEdit"];
}

- (IBAction)restartClicked:(id)sender {
	if (sender == factoryResetItem) {
		[NSApp activateIgnoringOtherApps:TRUE];
		NSAlert *alert = [[NSAlert alloc] init];
		[alert setMessageText:NSLocalizedString(@"FactoryReset.MessageText", nil)];
		[alert setInformativeText:NSLocalizedString(@"FactoryReset.InformativeText", nil)];
		[alert setAlertStyle:NSCriticalAlertStyle];
		NSButton* cancelButton = [alert addButtonWithTitle:NSLocalizedString(@"FactoryReset.Cancel", nil)];
		// we use middle invisible button to align the third RESET-button at left side
		[[alert addButtonWithTitle:@"Cancel"] setHidden:YES];
		[alert addButtonWithTitle:NSLocalizedString(@"FactoryReset.Reset", nil)];
		[alert.window makeFirstResponder:cancelButton];
		[cancelButton setKeyEquivalent:@"\E"];
		if ([alert runModal] != NSAlertThirdButtonReturn) {
			return;
		}
	}
	
	restarting = YES;
	resetting = sender == factoryResetItem;
	[self updateStatus];
	[daemonController restartInRecoveryMode: sender == restartRecoveryItem withFactoryReset: sender == factoryResetItem];
	daemonController.updateInterval = START_UPDATE_INTERVAL;

	restartTimer = [NSTimer timerWithTimeInterval:10.000 target:self selector:@selector(restartFailed) userInfo:nil repeats:NO];
	[[NSRunLoop currentRunLoop] addTimer:restartTimer forMode:NSRunLoopCommonModes];
}

- (void)welcomeContinue {
	DLog(@"welcomeContinue");
	BOOL autoStartWebUI = [[NSUserDefaults standardUserDefaults] boolForKey:@"AutoStartWebUI"];
	if (autoStartWebUI) {
		if (daemonController.connected) {
			if (daemonController.updateInterval == START_UPDATE_INTERVAL)
			{
				daemonController.updateInterval = NORMAL_UPDATE_INTERVAL;
			}
			[self showWebUI];
		} else {
			// try again in 100 msec for max. 25 seconds, then give up
			connectionAttempts++;
			if (connectionAttempts < 250) {
				[self performSelector:@selector(welcomeContinue) withObject:nil afterDelay: 0.100];
			} else {
				// show error message
				[self webuiClicked:nil];
			}
		}
	}
}

- (void)applicationWillTerminate:(NSNotification *)aNotification {
	DLog(@"Stop Daemon");
	[daemonController stop];
}

- (void)menuWillOpen:(NSMenu *)menu {
	DLog(@"menuWillOpen");
	daemonController.updateInterval = MENUOPEN_UPDATE_INTERVAL;
}

- (void)menuDidClose:(NSMenu *)menu {
	DLog(@"menuDidClose");
	daemonController.updateInterval = NORMAL_UPDATE_INTERVAL;
}

- (IBAction)infoLinkClicked:(id)sender {
	NSString *url;
	
	if (sender == homePageItem) {
		url = NSLocalizedString(@"Menu.LinkHomePage", nil);
	}
	else if (sender == downloadsItem) {
		url = NSLocalizedString(@"Menu.LinkDownloads", nil);
	}
	else if (sender == forumItem) {
		url = NSLocalizedString(@"Menu.LinkForum", nil);
	}

	[[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString:url]];
}

- (void)restartFailed {
	if (restarting) {
		restarting = NO;
		resetting = NO;
		daemonController.updateInterval = NORMAL_UPDATE_INTERVAL;
		[NSApp activateIgnoringOtherApps:TRUE];
		NSAlert *alert = [[NSAlert alloc] init];
		[alert setMessageText:NSLocalizedString(@"RestartNoConnection.MessageText", nil)];
		[alert setInformativeText:NSLocalizedString(@"RestartNoConnection.InformativeText", nil)];
		[alert setAlertStyle:NSWarningAlertStyle];
		[alert runModal];
	}
}

- (IBAction)showInFinderClicked:(id)sender {
	NSString* dir = nil;
	NSString* option = nil;

	if (sender == destDirItem) {
		option = @"DestDir";
	} else if (sender == interDirItem) {
		option = @"InterDir";
	} else if (sender == nzbDirItem) {
		option = @"NzbDir";
	} else if (sender == scriptDirItem) {
		option = @"ScriptDir";
	} else if (sender == logFileItem) {
		option = @"LogFile";
	} else if (sender == configFileItem) {
		dir = [daemonController configFilePath];
	} else if ([categoryItems containsObject:sender]) {
		int index = [categoryItems indexOfObject:sender];
		dir = [categoryDirs objectAtIndex:index];
	} else {
		return;
	}

	if (dir == nil) {
		NSString* mainDir = [[daemonController valueForOption:@"MainDir"] stringByExpandingTildeInPath];
		dir = [[daemonController valueForOption:option] stringByExpandingTildeInPath];
		dir = [dir stringByReplacingOccurrencesOfString:@"${MainDir}"
											 withString:mainDir
												options:NSCaseInsensitiveSearch
												  range:NSMakeRange(0, [dir length])];
	}
	
	dir = [[dir componentsSeparatedByCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@";,"]] objectAtIndex:0];

	if (dir.length == 0 || ![[NSFileManager defaultManager] fileExistsAtPath:dir]) {
		[NSApp activateIgnoringOtherApps:TRUE];
		NSAlert *alert = [[NSAlert alloc] init];
		[alert setMessageText:[NSString stringWithFormat:NSLocalizedString(@"CantShowInFinder.MessageText", nil), dir]];
		[alert setInformativeText:[NSString stringWithFormat:NSLocalizedString(option == nil ? @"CantShowInFinder.InformativeTextForCategory" : @"CantShowInFinder.InformativeTextWithOption", nil), option]];
		[alert runModal];
		return;
	}

	[[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[[NSURL fileURLWithPath:dir isDirectory:YES]]];
}

- (void)daemonStatusUpdated {
	if (restarting && daemonController.connected) {
		restarting = NO;
		[restartTimer invalidate];
		[NSApp activateIgnoringOtherApps:TRUE];
		NSAlert *alert = [[NSAlert alloc] init];
		[alert setMessageText:NSLocalizedString(resetting ? @"FactoryResetted.MessageText" : daemonController.recoveryMode ? @"RestartedRecoveryMode.MessageText" : @"Restarted.MessageText", nil)];
		[alert setInformativeText:NSLocalizedString(resetting ? @"FactoryResetted.InformativeText" : daemonController.recoveryMode ? @"RestartedRecoveryMode.InformativeText" : @"Restarted.InformativeText", nil)];
		[alert setAlertStyle:NSInformationalAlertStyle];
		[alert addButtonWithTitle:NSLocalizedString(@"Restarted.OK", nil)];
		[alert addButtonWithTitle:NSLocalizedString(@"Restarted.WebUI", nil)];
		if ([alert runModal] == NSAlertSecondButtonReturn) {
			[self showWebUI];
		}
		resetting = NO;
	} else {
		[self updateStatus];
	}
}

- (void)updateStatus {
	//DLog(@"updateStatus");
	
	NSString* info1 = @"";
	NSString* info2 = nil;
	BOOL preventSleep = NO;

	NSDictionary* status = [daemonController status];
	if (restarting || daemonController.restarting) {
		info1 = NSLocalizedString(@"Status.Restarting", nil);
	} else if (!daemonController.connected) {
		info1 = NSLocalizedString(@"Status.NoConnection", nil);
	} else if ([(NSNumber*)[status objectForKey:@"ServerStandBy"] integerValue] == 1) {
		if ([(NSNumber*)[status objectForKey:@"PostJobCount"] integerValue] > 0) {
			info1 = NSLocalizedString(@"Status.Post-Processing", nil);
			preventSleep = YES;
		}
		else if ([(NSNumber*)[status objectForKey:@"UrlCount"] integerValue] > 0) {
			info1 = NSLocalizedString(@"Status.Fetching NZBs", nil);
		}
		else if ([(NSNumber*)[status objectForKey:@"FeedActive"] integerValue] == 1) {
			info1 = NSLocalizedString(@"Status.Fetching Feeds", nil);
		}
		else if ([(NSNumber*)[status objectForKey:@"DownloadPaused"] integerValue] == 1 ||
				 [(NSNumber*)[status objectForKey:@"Download2Paused"] integerValue] == 1) {
			info1 = NSLocalizedString(@"Status.Paused", nil);
		} else {
			info1 = NSLocalizedString(@"Status.Idle", nil);
		}
	} else {
		int speed = [(NSNumber*)[status objectForKey:@"DownloadRate"] integerValue];
		if (speed >= 1024 * 1024 * 10) {
			info1 = [NSString stringWithFormat:NSLocalizedString(@"Status.DownloadingMB10", nil), speed / 1024 / 1024];
		}
		else if (speed >= 1024 * 1024) {
			info1 = [NSString stringWithFormat:NSLocalizedString(@"Status.DownloadingMB", nil), (float)speed / 1024.0 / 1024.0];
		}
		else {
			info1 = [NSString stringWithFormat:NSLocalizedString(@"Status.DownloadingKB", nil), speed / 1024];
		}
		preventSleep = YES;

		if (speed > 0) {
			long long remaining = ([(NSNumber*)[status objectForKey:@"RemainingSizeHi"] integerValue] << 32) + [(NSNumber*)[status objectForKey:@"RemainingSizeLo"] integerValue];
			int secondsLeft = remaining / speed;
			info2 = [NSString stringWithFormat:NSLocalizedString(@"Status.Left", nil), [self formatTimeLeft:secondsLeft]];
		}
	}

	if (preventSleep != preventingSleep) {
		[self updateSleepState:preventSleep];
	}

	[info1Item setTitle:info1];
	
	[info2Item setHidden:info2 == nil];
	if (info2 != nil) {
		[info2Item setTitle:info2];
	}
}

- (void)updateSleepState:(BOOL)preventSleep {
	if (preventSleep) {
		sleepID = 0;
		NSString* reason = NSLocalizedString(@"Status.PreventSleep", nil);
		IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleSystemSleep,
			kIOPMAssertionLevelOn, (__bridge CFStringRef)reason, &sleepID);
	}
	else if (sleepID != 0) {
		IOPMAssertionRelease(sleepID);
	}
	preventingSleep = preventSleep;
}

- (NSString*)formatTimeLeft:(int)sec {
	int days = floor(sec / 86400);
	int hours = floor((sec % 86400) / 3600);
	int minutes = floor((sec / 60) % 60);
	int seconds = floor(sec % 60);
	
	if (days > 10)
	{
		return [NSString stringWithFormat:NSLocalizedString(@"Left.Days", nil), days];
	}
	if (days > 0)
	{
		return [NSString stringWithFormat:NSLocalizedString(@"Left.Days.Hours", nil), days, hours];
	}
	if (hours > 10)
	{
		return [NSString stringWithFormat:NSLocalizedString(@"Left.Hours", nil), hours];
	}
	if (hours > 0)
	{
		return [NSString stringWithFormat:NSLocalizedString(@"Left.Hours.Minutes", nil), hours, minutes];
	}
	if (minutes > 10)
	{
		return [NSString stringWithFormat:NSLocalizedString(@"Left.Minutes", nil), minutes];
	}
	if (minutes > 0)
	{
		return [NSString stringWithFormat:NSLocalizedString(@"Left.Minutes.Seconds", nil), minutes, seconds];
	}
	
	return [NSString stringWithFormat:NSLocalizedString(@"Left.Seconds", nil), seconds];
}

- (void)daemonConfigLoaded {
	DLog(@"config loaded");
	[self updateCategoriesMenu];
}

- (void)updateCategoriesMenu {
	NSMenu *submenu = destDirItem.parentItem.submenu;

	for (NSMenuItem* item in categoryItems) {
		[submenu removeItem:item];
	}
	
	categoryItems = [NSMutableArray array];
	categoryDirs = [NSMutableArray array];

	NSString* mainDir = [[daemonController valueForOption:@"MainDir"] stringByExpandingTildeInPath];
	NSString* destDir = [[daemonController valueForOption:@"DestDir"] stringByExpandingTildeInPath];

	for (int i=1; ; i++) {
		NSString* catName = [daemonController valueForOption:[NSString stringWithFormat:@"Category%i.Name", i]];
		NSString* catDir = [daemonController valueForOption:[NSString stringWithFormat:@"Category%i.DestDir", i]];

		if (catName.length == 0) {
			break;
		}
		
		if (catDir.length == 0) {
			catDir = [destDir stringByAppendingPathComponent:catName];
		}

		NSString* dir = [catDir stringByExpandingTildeInPath];
		dir = [dir stringByReplacingOccurrencesOfString:@"${MainDir}"
											 withString:mainDir
												options:NSCaseInsensitiveSearch
												  range:NSMakeRange(0, dir.length)];
		dir = [dir stringByReplacingOccurrencesOfString:@"${DestDir}"
											 withString:destDir
												options:NSCaseInsensitiveSearch
												  range:NSMakeRange(0, dir.length)];
		
		NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithFormat:@"Category%i: %@", i, catName]
													  action:@selector(showInFinderClicked:) keyEquivalent:@""];
		[item setTarget:self];
		[submenu insertItem:item atIndex:[submenu indexOfItem:destDirSeparator]];
		
		[categoryItems addObject:item];
		[categoryDirs addObject:dir];
	}
}

@end
