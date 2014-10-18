//
//  SMCManager.swift
//  Thermo
//
//  Created by Naoya Sato on 10/19/14.
//  Copyright (c) 2014 Naoya Sato. All rights reserved.
//

import Foundation

class SMCManager {
    var conn: io_connect_t = 0
    class var sharedManager : SMCManager {
        struct Static {
            static let instance : SMCManager = SMCManager()
        }
        return Static.instance
    }
    
    init () {
        SMCOpen(&conn)
    }
    
    deinit {
        SMCClose(conn)
    }
    
    func getTemparature() -> UInt8 {
        let ckey = strdup("TC0F")
        var temp = SMCGetTemparature(ckey, conn)
        return UInt8(temp)
    }
}