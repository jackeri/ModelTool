#include "app_icon.h"
#include "embedded_files.h"

#import <Cocoa/Cocoa.h>

void mt::setAppIcon(GLFWwindow *)
{
	@autoreleasepool
	{
		// The Dock takes the bundle icon only when the app is launched through the bundle (Finder, open), not when the
		// executable is started directly (IDE, terminal), so the icon is set when the program runs
		NSImage *image = nil;
		NSString *path = [[NSBundle mainBundle] pathForResource:@"icon" ofType:@"icns"];
		if (path)
		{
			image = [[[NSImage alloc] initWithContentsOfFile:path] autorelease];
		}

		if (!image)
		{
			const EmbeddedFile png = embeddedFile("icon.png");
			if (png.data)
			{
				NSData *data = [NSData dataWithBytes:png.data length:png.size];
				image = [[[NSImage alloc] initWithData:data] autorelease];
			}
		}

		if (image)
		{
			[NSApp setApplicationIconImage:image];
		}
	}
}
