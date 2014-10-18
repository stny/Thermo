//
//  StatusBar.swift
//  Thermo
//
//  Created by Naoya Sato on 10/20/14.
//  Copyright (c) 2014 Naoya Sato. All rights reserved.
//

import Foundation
import Cocoa

class StatusBar : NSObject, NSMenuDelegate {
    // A linker error on Xcode 6.1. Not available NSVariableStatusItemLength :(
    let statusItem = NSStatusBar.systemStatusBar().statusItemWithLength(-1)
    let statusMenu = NSMenu()

    let manager = SMCManager.sharedManager
    
    var timer, menuTimer : dispatch_source_t?

    override init () {
        super.init()
        self.statusMenu.delegate = self
        self.statusItem.menu = statusMenu
        self.statusItem.highlightMode = true
        menuSetup()
        
        let icon = NSImage(named: "Status")
        //icon!.setTemplate(true)
        self.statusItem.image = icon
        
        self.timer = createDispatchTimer(5 * NSEC_PER_SEC,0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), buttonImageUpdate)
        self.menuTimer = createDispatchTimer(NSEC_PER_SEC, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), menuTitleUpdate)
        dispatch_suspend(self.menuTimer)
    }

// MARK: -
// MARK: Private methods
    
    private func menuSetup() {
        self.statusItem.menu!.addItemWithTitle("42 \u{00b0}C", action: nil, keyEquivalent: "")
        self.statusItem.menu!.addItemWithTitle("Open Activity Monitor", action: Selector("launchActivityMonitor:"), keyEquivalent:"")
        self.statusItem.menu!.addItem(NSMenuItem.separatorItem())
        self.statusItem.menu!.addItemWithTitle("Quit Thermo", action: Selector("terminate:"), keyEquivalent: "")
    }
    
    private func getOSVersion () -> String {
        let versionString = NSProcessInfo.processInfo().operatingSystemVersionString
        return split(versionString, { $0 == " " })[1]
    }
    
    private func buttonImageUpdate() {
        var temparature = self.manager.getTemparature()
        dispatch_async(dispatch_get_main_queue(), {
            if (temparature >= 70) {
                self.statusItem.image = NSImage(named: "StatusHot")
            } else if (temparature >= 60) {
                self.statusItem.image = NSImage(named: "StatusWarm")
            } else {
                self.statusItem.image = NSImage(named: "Status")
            }
        })
    }
    
    private func menuTitleUpdate() {
        var temparature = self.manager.getTemparature()
        var item = self.statusItem.menu!.itemAtIndex(0)
        dispatch_async(dispatch_get_main_queue(), {
            item!.title = String(format: "%d \u{00b0}C", temparature)
        })
    }

// MARK: -
// MARK: Menu methods
    
    func menuWillOpen(menu: NSMenu) {
        dispatch_resume(self.menuTimer)
    }
    
    func menuDidClose(menu: NSMenu) {
        dispatch_suspend(self.menuTimer)
    }
    
}