#include <GLFW/glfw3.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

void* get_metal_layer(GLFWwindow* win, void* window) {
    NSWindow* nsWindow = (NSWindow*)window;
    NSView* nsView = [nsWindow contentView];
    [nsView setWantsLayer:YES];

    // Set CAMetalLayer as the backing layer if it's not already
    if (![nsView.layer isKindOfClass:[CAMetalLayer class]]) {
        int width, height;
        glfwGetFramebufferSize(win, &width, &height);

        CGFloat scale = nsWindow.screen ? nsWindow.screen.backingScaleFactor : NSScreen.mainScreen.backingScaleFactor;
        CAMetalLayer* metalLayer = [CAMetalLayer layer];
        [metalLayer setContentsScale:scale];
        [metalLayer setDrawableSize:CGSizeMake(width, height)];
        [nsView setLayer:metalLayer];
    }

    return (CAMetalLayer*)nsView.layer;;
}
