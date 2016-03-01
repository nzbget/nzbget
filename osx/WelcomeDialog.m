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

#import "WelcomeDialog.h"

@implementation WelcomeDialog

- (id)init {
	self = [super initWithWindowNibName:@"WelcomeDialog"];
	return self;
}

- (void)setMainDelegate:(id)mainDelegate{
    _mainDelegate = mainDelegate;
}

- (void)showDialog {
    BOOL doNotShowWelcomeDialog = [[NSUserDefaults standardUserDefaults] boolForKey:@"DoNotShowWelcomeDialog"];
    if (doNotShowWelcomeDialog) {
		[_mainDelegate performSelector:@selector(welcomeContinue)];
        return;
    }
    
    DLog(@"creating window");
	NSWindow *window = [self window];
	DLog(@"creating window - END");

	// set file icon
    NSImage *image = [NSImage imageNamed:@"mainicon.icns"];
	[_imageView setImage:image];
	
	// load warning icon
	[_badgeView setImage:[[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeForHFSTypeCode(kAlertNoteIcon)]];
	
    [[_messageText textStorage] readFromURL:[NSURL fileURLWithPath:[[NSBundle mainBundle] pathForResource:@"Welcome" ofType:@"rtf"]] options:nil documentAttributes:nil];
    
	// adjust height of text control and window
	[[_messageText layoutManager] ensureLayoutForTextContainer:[_messageText textContainer]];
    
	NSSize scrollSize = [_messageTextScrollView frame].size;
	float deltaHeight = [_messageText frame].size.height - scrollSize.height + 6;
	if (deltaHeight < 0) {
		deltaHeight = 0;
	}

	NSSize winSize = [[window contentView] frame ].size;
	winSize.height += deltaHeight;
	[window setContentSize:winSize];

	// set active control
	[window makeFirstResponder:_okButton];

	// show modal dialog
    [NSApp activateIgnoringOtherApps:TRUE];
	[window center];
	[self showWindow:nil];
}

- (IBAction)okButtonPressed:(id)sender {
	[self close];
	[_mainDelegate performSelector:@selector(welcomeContinue)];
}

@end
