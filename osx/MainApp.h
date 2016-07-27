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
#import <IOKit/pwr_mgt/IOPMLib.h>
#import "DaemonController.h"

@interface MainApp : NSObject <NSMenuDelegate, DaemonControllerDelegate> {
	IBOutlet NSMenu *statusMenu;
    NSStatusItem *statusItem;
	IBOutlet NSMenuItem *webuiItem;
	IBOutlet NSMenuItem *homePageItem;
	IBOutlet NSMenuItem *downloadsItem;
	IBOutlet NSMenuItem *forumItem;
	IBOutlet NSMenuItem *info1Item;
	IBOutlet NSMenuItem *info2Item;
	IBOutlet NSMenuItem *restartRecoveryItem;
	IBOutlet NSMenuItem *factoryResetItem;
	IBOutlet NSMenuItem *destDirItem;
	IBOutlet NSMenuItem *interDirItem;
	IBOutlet NSMenuItem *nzbDirItem;
	IBOutlet NSMenuItem *scriptDirItem;
	IBOutlet NSMenuItem *configFileItem;
	IBOutlet NSMenuItem *logFileItem;
	IBOutlet NSMenuItem *destDirSeparator;
	NSWindowController *welcomeDialog;
	NSWindowController *preferencesDialog;
	DaemonController *daemonController;
	int connectionAttempts;
	BOOL restarting;
	BOOL resetting;
	BOOL preventingSleep;
	IOPMAssertionID sleepID;
	NSTimer* restartTimer;
	NSMutableArray* categoryItems;
	NSMutableArray* categoryDirs;
}

+ (void)setupAppDefaults;

- (void)setupDefaultsObserver;

- (IBAction)quitClicked:(id)sender;

- (IBAction)preferencesClicked:(id)sender;

- (void)userDefaultsDidChange:(id)sender;

- (IBAction)aboutClicked:(id)sender;

+ (BOOL)wasLaunchedAsLoginItem;

- (IBAction)webuiClicked:(id)sender;

- (IBAction)infoLinkClicked:(id)sender;

- (IBAction)openConfigInTextEditClicked:(id)sender;

- (IBAction)restartClicked:(id)sender;

- (IBAction)showInFinderClicked:(id)sender;

- (void)updateSleepState:(BOOL)preventSleep;

@end
