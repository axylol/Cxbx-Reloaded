// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
// ******************************************************************
// *   src->devices->chihiro->MediaBoard.cpp
// *
// *  This file is part of the Cxbx project.
// *
// *  Cxbx and Cxbe are free software; you can redistribute them
// *  and/or modify them under the terms of the GNU General Public
// *  License as published by the Free Software Foundation; either
// *  version 2 of the license, or (at your option) any later version.
// *
// *  This program is distributed in the hope that it will be useful,
// *  but WITHOUT ANY WARRANTY; without even the implied warranty of
// *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// *  GNU General Public License for more details.
// *
// *  You should have recieved a copy of the GNU General Public License
// *  along with this program; see the file COPYING.
// *  If not, write to the Free Software Foundation, Inc.,
// *  59 Temple Place - Suite 330, Bostom, MA 02111-1307, USA.
// *
// *  (c) 2019 Luke Usher
// *
// *  All rights reserved
// *
// ******************************************************************

#include "MediaBoard.h"
#include <cstdio>
#include <string>

#define _XBOXKRNL_DEFEXTRN_
#define LOG_PREFIX CXBXR_MODULE::JVS // TODO: XBAM


#include <core\kernel\exports\xboxkrnl.h>
#include "core\kernel\init\\CxbxKrnl.h"
#include "core\kernel\exports\EmuKrnl.h" // for HalSystemInterrupts

chihiro_bootid &MediaBoard::GetBootId()
{
    return BootID;
}

void MediaBoard::SetMountPath(std::string path)
{
    m_MountPath = path;

    // Load Boot.id from file
    FILE* bootidFile = fopen((path+"/boot.id").c_str(), "rb");
    if (bootidFile == nullptr) {
        CxbxrAbort("Could not open Chihiro boot.id");
    }
    fread(&BootID, 1, sizeof(chihiro_bootid), bootidFile);
    fclose(bootidFile);
}

uint32_t MediaBoard::LpcRead(uint32_t addr, int size)
{
    if (addr == 0x40E0)
        return 1;

    if (addr == 0x401E) // Firmware Version Number
        return 0x0317;
    if (addr == 0x4020) // XBAM String (SEGABOOT reports Media Board is not present if these values change)
        return 0x00A0;
    if (addr == 0x40F4)
        return 0x0003;

    if (addr == 0x4026)
        return temp_0x4026;

    if (addr == 0x40F0) // Media board type (1)
        return 0x0000;
    if (addr == 0x4022)
        return 0x4258;
    if (addr == 0x4024)
        return 0x4D41;
    if (addr == 0x4084)
        return 0;

	printf("MediaBoard::LpcRead: Unknown Addr %08X %d\n", addr, size);
	return 0;
}

void MediaBoard::LpcWrite(uint32_t addr, uint32_t value, int size)
{
    if (addr == 0x40E1) {
        HalSystemInterrupts[10].Assert(false);
        return;
    }

    if (addr == 0x4026) {
        temp_0x4026 = value;
        return;
    }

    printf("MediaBoard::LpcWrite: Unknown Addr %08X = %08X\n", addr, value);
}

void MediaBoard::ComRead(uint32_t offset, void* buffer, uint32_t length)
{
    // checksum = 55d50
    if (offset == 0x00D00000) { // idk if correct
        /*uint8_t data[864]; // TODO: figure our what this is

        uint8_t* pbVar8  = (uint8_t*)(data + 4);
        uint32_t checksum = 0;

        int i = 0x90;
        do {
            uint8_t* pbVar1 = pbVar8 + -3;
            uint8_t* pbVar2 = pbVar8 + -4;
            uint8_t* pbVar3 = pbVar8 + -2;
            uint8_t* pbVar4 = pbVar8 + -1;
            uint8_t* pbVar5 = pbVar8 + 1;
            uint8_t bVar6 = *pbVar8;
            uint8_t* pbVar8 = pbVar8 + 6;
            i--;
            checksum = (uint32_t)bVar6 + checksum +
                (uint32_t)*pbVar2 + (uint32_t)*pbVar1 + (uint32_t)*pbVar3 + (uint32_t)*pbVar4 + (uint32_t)*pbVar5;
        } while (i != 0);*/

        printf("0x00D00000\n");
        return;
    }

    if (offset == 0x005FFCE9) { // ret from cmd 0x0104 idk if correct
        memset(buffer, 0, length);
        *(uint32_t*)buffer = 0x10000;
        return;
    }

    if (offset == 0x800000 || offset == 0x800200) {
        memcpy(buffer, buffer_800000, length);
        return;
    }

    if (offset == 0x900000 || offset == 0x900200) {
        memcpy(buffer, buffer_900000, length);
        return;
    }

    // Copy the current read buffer to the output
    printf("ComRead %08X %d\n", offset, length);
    memset(buffer, 0, length);
}

void MediaBoard::ComWrite(uint32_t offset, void* buffer, uint32_t length)
{
    if (offset == 0x005FFCE9) { // idk if correct
        HalSystemInterrupts[10].Assert(true);
        return;
    }

    // network related?
    if (offset == 0x800000) {
        // guessed
        memcpy(buffer_800000, buffer, length);
        HalSystemInterrupts[10].Assert(true);
        return;
    }
    if (offset == 0x800200) {
        // guessed
        // input = string of an ip address

        uint8_t* outputBuffer = (uint8_t*)buffer_800000;
        *(uint32_t*)outputBuffer = 167772161; // 10.0.0.1

        HalSystemInterrupts[10].Assert(true);
        return;
    }

    if (offset == 0x900000) {
        memcpy(buffer_900000, buffer, length);
        HalSystemInterrupts[10].Assert(true);
        return;
    }
    if (offset == 0x900200) {
        uint8_t tempBuffer[512];
        memcpy(tempBuffer, buffer, length);

        uint8_t* inputBuffer = (uint8_t*)tempBuffer;
        uint8_t* outputBuffer = (uint8_t*)buffer_900000;

        uint32_t command = *(uint16_t*)&inputBuffer[2];
        if (command == 0)
            return;

        bool success = true;
        switch (command) {
        case MB_CMD_DIMM_SIZE: { // 0x0001
            // sent on 11d910
            // response on 11d980
            *(uint32_t*)&outputBuffer[4] = 1024 * ONE_MB;
            break;
        }
        case MB_CMD_STATUS: { // 0x0100
            *(uint32_t*)&outputBuffer[4] = MB_STATUS_READY;
            *(uint32_t*)&outputBuffer[8] = 100; // Load/Test Percentage (0-100)
            break;
        }
        case MB_CMD_FIRMWARE_VERSION: { // 0x0101
            // reads 2 uint16_t from + 6 and + 8
            *(uint32_t*)&outputBuffer[4] = 0x0317;
            break;
        }
        case MB_CMD_SYSTEM_TYPE: { // 0x0102
            *(uint32_t*)&outputBuffer[4] = MB_SYSTEM_TYPE_DEVELOPER | MB_SYSTEM_TYPE_GDROM;
            break;
        }
        case MB_CMD_SERIAL_NUMBER: { // 0x0103
            memcpy(&outputBuffer[4], "A89E-25A47354512", 16);
            break;
        }
        case 0x0104: {
            //printf("0x0104\n");
            // seems to be followed with mbcom read?
            // processed in FUN_0011dcf0
            *(uint32_t*)&outputBuffer[4] = 0;
            break;
        }
        case MB_CMD_HARDWARE_TEST: {
            uint32_t testType = *(uint32_t*)&inputBuffer[4];
            xbox::addr_xt resultWritePtr = *(uint32_t*)&inputBuffer[8];
            *(uint32_t*)&outputBuffer[4] = *(uint32_t*)&inputBuffer[4];

            printf("Perform Test Type %X, place result at %08X\n", testType, resultWritePtr);

            // For now, just pretend we did the test and was successful
            // TODO: How to report percentage? Get's stuck on "CHECKING 0% but still shows "TEST OK"
            memcpy((void*)resultWritePtr, "TEST OK", 8);
        } break;
        case 0x0204: {
            //printf("0x0204\n");
            *(uint32_t*)&outputBuffer[4] = 0;
            break;
        }
        case 0x0415: {
            //printf("0x0415\n");
            *(uint32_t*)&outputBuffer[4] = 167772161;
            break;
        }
        case 0x0601: {
            //printf("0x0601\n");
            *(uint32_t*)&outputBuffer[4] = 0;
            break;
        }
        case 0x0602: { // those who know
            //printf("0x0602\n");

            // This triggers 0x0605
            *(uint32_t*)&outputBuffer[4] = 0xffff;
            break;
        }
        case 0x0605: {
            //printf("0x0605\n");
            *(uint32_t*)&outputBuffer[4] = 0;
            break;
        }
        case 0x0606: { // related to network?
            //printf("0x0606\n");
            *(uint32_t*)&outputBuffer[4] = 0;
            break;
        }
        case 0x0607: {
            //printf("0x0607 %d %d %d\n", *(uint16_t*)&inputBuffer[4], *(uint16_t*)&inputBuffer[6], *(uint32_t*)&inputBuffer[8]);

            *(uint32_t*)&outputBuffer[4] = 0;
            *(uint32_t*)&outputBuffer[8] = 0;
            break;
        }
        case 0x0608: {
            //printf("0x0608\n");

            // is read on 5673c
            *(uint32_t*)&outputBuffer[4] = 167772161;
            break;
        }
        default: {
            printf("Unhandled MediaBoard Command: %04X\n", command);
            break;
        }
        }

        *(uint16_t*)outputBuffer = *(uint16_t*)inputBuffer; // some kind of sequence?
        if (success)
            *(uint16_t*)&outputBuffer[2] = command | 0x8000;
        else
            *(uint16_t*)&outputBuffer[2] = 0x8000;

        HalSystemInterrupts[10].Assert(true);
        return;
    }

    printf("Unhandled MediaBoard mbcom: offset %08X %d\n", offset, length);
}
