//
//  AppDelegate.swift
//  Thermo
//
//  Created by Naoya Sato on 10/19/14.
//  Copyright (c) 2014 Naoya Sato. All rights reserved.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {
    let statusBar = StatusBar()
    
    func applicationDidFinishLaunching(aNotification: NSNotification) {
    }

    func applicationWillTerminate(aNotification: NSNotification) {
    }
    
    func launchActivityMonitor(sender: NSMenuItem) {
        NSWorkspace.sharedWorkspace().launchApplication("/Applications/Utilities/Activity Monitor.app")
    }

}

