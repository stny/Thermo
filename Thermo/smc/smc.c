/*
 * Apple System Management Control (SMC) Tool 
 * Copyright (C) 2006 devnull 
 * Portions Copyright (C) 2013 Michael Wilber
 * Portions Copyright (C) 2014 Naoya Sato
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <IOKit/IOKitLib.h>
#include "smc.h"
#include <libkern/OSAtomic.h>

// Cache the keyInfo to lower the energy impact of SMCReadKey() / SMCReadKey2()
#define KEY_INFO_CACHE_SIZE 100
struct {
    UInt32 key;
    SMCKeyData_keyInfo_t keyInfo;
} g_keyInfoCache[KEY_INFO_CACHE_SIZE];

int g_keyInfoCacheCount = 0;
OSSpinLock g_keyInfoSpinLock = 0;

kern_return_t SMCCall2(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure, io_connect_t conn);

#pragma mark C Helpers

UInt32 _strtoul(char *str, int size, int base)
{
    UInt32 total = 0;
    int i;

    for (i = 0; i < size; i++)
    {
        if (base == 16)
            total += str[i] << (size - 1 - i) * 8;
        else
           total += ((unsigned char) (str[i]) << (size - 1 - i) * 8);
    }
    return total;
}

void _ultostr(char *str, UInt32 val)
{
    str[0] = '\0';
    sprintf(str, "%c%c%c%c", 
            (unsigned int) val >> 24,
            (unsigned int) val >> 16,
            (unsigned int) val >> 8,
            (unsigned int) val);
}

float _strtof(unsigned char *str, int size, int e)
{
    float total = 0;
    int i;
    
    for (i = 0; i < size; i++)
    {
        if (i == (size - 1))
            total += (str[i] & 0xff) >> e;
        else
            total += str[i] << (size - 1 - i) * (8 - e);
    }
    
	total += (str[size-1] & 0x03) * 0.25;
    
    return total;
}


void printFP1F(SMCVal_t val)
{
    printf("%.5f ", ntohs(*(UInt16*)val.bytes) / 32768.0);
}

void printFP4C(SMCVal_t val)
{
    printf("%.5f ", ntohs(*(UInt16*)val.bytes) / 4096.0);
}

void printFP5B(SMCVal_t val)
{
    printf("%.5f ", ntohs(*(UInt16*)val.bytes) / 2048.0);
}

void printFP6A(SMCVal_t val)
{
    printf("%.4f ", ntohs(*(UInt16*)val.bytes) / 1024.0);
}

void printFP79(SMCVal_t val)
{
    printf("%.4f ", ntohs(*(UInt16*)val.bytes) / 512.0);
}

void printFP88(SMCVal_t val)
{
    printf("%.3f ", ntohs(*(UInt16*)val.bytes) / 256.0);
}

void printFPA6(SMCVal_t val)
{
    printf("%.2f ", ntohs(*(UInt16*)val.bytes) / 64.0);
}

void printFPC4(SMCVal_t val)
{
    printf("%.2f ", ntohs(*(UInt16*)val.bytes) / 16.0);
}

void printFPE2(SMCVal_t val)
{
    printf("%.2f ", ntohs(*(UInt16*)val.bytes) / 4.0);
}

void printUInt(SMCVal_t val)
{
    printf("%u ", (unsigned int) _strtoul((char *)val.bytes, val.dataSize, 10));
}

void printSP1E(SMCVal_t val)
{
    printf("%.5f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 16384.0);
}

void printSP3C(SMCVal_t val)
{
    printf("%.5f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 4096.0);
}

void printSP4B(SMCVal_t val)
{
    printf("%.4f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 2048.0);
}

void printSP5A(SMCVal_t val)
{
    printf("%.4f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 1024.0);
}

void printSP69(SMCVal_t val)
{
    printf("%.3f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 512.0);
}

void printSP78(SMCVal_t val)
{
    printf("%.3f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 256.0);
}

void printSP87(SMCVal_t val)
{
    printf("%.3f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 128.0);
}

void printSP96(SMCVal_t val)
{
    printf("%.2f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 64.0);
}

void printSPB4(SMCVal_t val)
{
    printf("%.2f ", ((SInt16)ntohs(*(UInt16*)val.bytes)) / 16.0);
}

void printSPF0(SMCVal_t val)
{
    printf("%.0f ", (float)ntohs(*(UInt16*)val.bytes));
}

void printSI8(SMCVal_t val)
{
    printf("%d ", (signed char)*val.bytes);
}

void printSI16(SMCVal_t val)
{
    printf("%d ", ntohs(*(SInt16*)val.bytes));
}

void printPWM(SMCVal_t val)
{
    printf("%.1f%% ", ntohs(*(UInt16*)val.bytes) * 100 / 65536.0);
}

void printBytesHex(SMCVal_t val)
{
    int i;

    printf("(bytes");
    for (i = 0; i < val.dataSize; i++)
        printf(" %02x", (unsigned char) val.bytes[i]);
    printf(")\n");
}

void printVal(SMCVal_t val)
{
    printf("  %-4s  [%-4s]  ", val.key, val.dataType);
    if (val.dataSize > 0)
    {
        if ((strcmp(val.dataType, DATATYPE_UINT8) == 0) ||
            (strcmp(val.dataType, DATATYPE_UINT16) == 0) ||
            (strcmp(val.dataType, DATATYPE_UINT32) == 0))
            printUInt(val);
        else if (strcmp(val.dataType, DATATYPE_FP1F) == 0 && val.dataSize == 2)
            printFP1F(val);
        else if (strcmp(val.dataType, DATATYPE_FP4C) == 0 && val.dataSize == 2)
            printFP4C(val);
        else if (strcmp(val.dataType, DATATYPE_FP5B) == 0 && val.dataSize == 2)
            printFP5B(val);
        else if (strcmp(val.dataType, DATATYPE_FP6A) == 0 && val.dataSize == 2)
            printFP6A(val);
        else if (strcmp(val.dataType, DATATYPE_FP79) == 0 && val.dataSize == 2)
            printFP79(val);
        else if (strcmp(val.dataType, DATATYPE_FP88) == 0 && val.dataSize == 2)
            printFP88(val);
        else if (strcmp(val.dataType, DATATYPE_FPA6) == 0 && val.dataSize == 2)
            printFPA6(val);
        else if (strcmp(val.dataType, DATATYPE_FPC4) == 0 && val.dataSize == 2)
            printFPC4(val);
        else if (strcmp(val.dataType, DATATYPE_FPE2) == 0 && val.dataSize == 2)
            printFPE2(val);
		else if (strcmp(val.dataType, DATATYPE_SP1E) == 0 && val.dataSize == 2)
			printSP1E(val);
		else if (strcmp(val.dataType, DATATYPE_SP3C) == 0 && val.dataSize == 2)
			printSP3C(val);
		else if (strcmp(val.dataType, DATATYPE_SP4B) == 0 && val.dataSize == 2)
			printSP4B(val);
		else if (strcmp(val.dataType, DATATYPE_SP5A) == 0 && val.dataSize == 2)
			printSP5A(val);
		else if (strcmp(val.dataType, DATATYPE_SP69) == 0 && val.dataSize == 2)
			printSP69(val);
		else if (strcmp(val.dataType, DATATYPE_SP78) == 0 && val.dataSize == 2)
			printSP78(val);
		else if (strcmp(val.dataType, DATATYPE_SP87) == 0 && val.dataSize == 2)
			printSP87(val);
		else if (strcmp(val.dataType, DATATYPE_SP96) == 0 && val.dataSize == 2)
			printSP96(val);
		else if (strcmp(val.dataType, DATATYPE_SPB4) == 0 && val.dataSize == 2)
			printSPB4(val);
		else if (strcmp(val.dataType, DATATYPE_SPF0) == 0 && val.dataSize == 2)
			printSPF0(val);
		else if (strcmp(val.dataType, DATATYPE_SI8) == 0 && val.dataSize == 1)
			printSI8(val);
		else if (strcmp(val.dataType, DATATYPE_SI16) == 0 && val.dataSize == 2)
			printSI16(val);
		else if (strcmp(val.dataType, DATATYPE_PWM) == 0 && val.dataSize == 2)
			printPWM(val);

        printBytesHex(val);
    }
    else
    {
            printf("no data\n");
    }
}

#pragma mark Shared SMC functions

kern_return_t SMCOpen(io_connect_t *conn)
{
    kern_return_t result;
    mach_port_t   masterPort;
    io_iterator_t iterator;
    io_object_t   device;
    
	IOMasterPort(MACH_PORT_NULL, &masterPort);
    
    CFMutableDictionaryRef matchingDictionary = IOServiceMatching("AppleSMC");
    result = IOServiceGetMatchingServices(masterPort, matchingDictionary, &iterator);
    if (result != kIOReturnSuccess)
    {
        printf("Error: IOServiceGetMatchingServices() = %08x\n", result);
        return 1;
    }
    
    device = IOIteratorNext(iterator);
    IOObjectRelease(iterator);
    if (device == 0)
    {
        printf("Error: no SMC found\n");
        return 1;
    }
    
    result = IOServiceOpen(device, mach_task_self(), 0, conn);
    IOObjectRelease(device);
    if (result != kIOReturnSuccess)
    {
        printf("Error: IOServiceOpen() = %08x\n", result);
        return 1;
    }
    
    return kIOReturnSuccess;
}

kern_return_t SMCClose(io_connect_t conn)
{
    return IOServiceClose(conn);
}

kern_return_t SMCCall2(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure,io_connect_t conn)
{
    size_t   structureInputSize;
    size_t   structureOutputSize;
    structureInputSize = sizeof(SMCKeyData_t);
    structureOutputSize = sizeof(SMCKeyData_t);
    
    return IOConnectCallStructMethod(conn, index, inputStructure, structureInputSize, outputStructure, &structureOutputSize);
}

// Provides key info, using a cache to dramatically improve the energy impact of smcFanControl
kern_return_t SMCGetKeyInfo(UInt32 key, SMCKeyData_keyInfo_t* keyInfo, io_connect_t conn)
{
    SMCKeyData_t inputStructure;
    SMCKeyData_t outputStructure;
    kern_return_t result = kIOReturnSuccess;
    int i = 0;
    
    OSSpinLockLock(&g_keyInfoSpinLock);
    
    for (; i < g_keyInfoCacheCount; ++i)
    {
        if (key == g_keyInfoCache[i].key)
        {
            *keyInfo = g_keyInfoCache[i].keyInfo;
            break;
        }
    }
    
    if (i == g_keyInfoCacheCount)
    {
        // Not in cache, must look it up.
        memset(&inputStructure, 0, sizeof(inputStructure));
        memset(&outputStructure, 0, sizeof(outputStructure));
        
        inputStructure.key = key;
        inputStructure.data8 = SMC_CMD_READ_KEYINFO;
        
        result = SMCCall2(KERNEL_INDEX_SMC, &inputStructure, &outputStructure, conn);
        if (result == kIOReturnSuccess)
        {
            *keyInfo = outputStructure.keyInfo;
            if (g_keyInfoCacheCount < KEY_INFO_CACHE_SIZE)
            {
                g_keyInfoCache[g_keyInfoCacheCount].key = key;
                g_keyInfoCache[g_keyInfoCacheCount].keyInfo = outputStructure.keyInfo;
                ++g_keyInfoCacheCount;
            }
        }
    }
    
    OSSpinLockUnlock(&g_keyInfoSpinLock);
    
    return result;
}

kern_return_t SMCReadKey2(UInt32Char_t key, SMCVal_t *val,io_connect_t conn)
{
    kern_return_t result;
    SMCKeyData_t  inputStructure;
    SMCKeyData_t  outputStructure;
    
    memset(&inputStructure, 0, sizeof(SMCKeyData_t));
    memset(&outputStructure, 0, sizeof(SMCKeyData_t));
    memset(val, 0, sizeof(SMCVal_t));
    
    inputStructure.key = _strtoul(key, 4, 16);
    sprintf(val->key, "%s", key);
    
    result = SMCGetKeyInfo(inputStructure.key, &outputStructure.keyInfo, conn);
    if (result != kIOReturnSuccess)
    {
        return result;
    }
    
    val->dataSize = outputStructure.keyInfo.dataSize;
    _ultostr(val->dataType, outputStructure.keyInfo.dataType);
    inputStructure.keyInfo.dataSize = val->dataSize;
    inputStructure.data8 = SMC_CMD_READ_BYTES;
    
    result = SMCCall2(KERNEL_INDEX_SMC, &inputStructure, &outputStructure,conn);
    if (result != kIOReturnSuccess)
    {
        return result;
    }
    
    memcpy(val->bytes, outputStructure.bytes, sizeof(outputStructure.bytes));
    
    return kIOReturnSuccess;
}

float SMCGetTemparature(UInt32Char_t key, io_connect_t conn) {
    SMCVal_t val;
    SMCReadKey2(key, &val, conn);
    return ((val.bytes[0] * 256 + val.bytes[1]) >> 2)/64;
}