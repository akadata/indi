/*
    ZWO EFA Focuser
    Copyright (C) 2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#include "asi_focuser.h"
#include "EAF_focuser.h"
#include "indicom.h"

#include <cmath>
#include <cstring>
#include <memory>

#include <termios.h>
#include <unistd.h>

#define MAX_DEVICES 4

static int iAvailableFocusersCount;
static ASIEAF * focusers[MAX_DEVICES];

void ASI_EAF_ISInit()
{
    static bool isInit = false;
    if (!isInit)
    {
        iAvailableFocusersCount = 0;

        iAvailableFocusersCount = EAFGetNum();
        if (iAvailableFocusersCount > MAX_DEVICES)
            iAvailableFocusersCount = MAX_DEVICES;

        if (iAvailableFocusersCount <= 0)
        {
            IDLog("No ASI EAF detected.");
        }
        else
        {
            int iAvailableFocusersCount_ok = 0;
            for (int i = 0; i < iAvailableFocusersCount; i++)
            {
                int id;
                EAF_ERROR_CODE result = EAFGetID(i, &id);
                if (result != EAF_SUCCESS)
                {
                    IDLog("ERROR: ASI EAF %d EAFGetID error %d.", i + 1, result);
                    continue;
                }
                EAF_INFO info;
                result = EAFGetProperty(id, &info);
                if (result != EAF_SUCCESS && result != EAF_ERROR_CLOSED)
                {
                    IDLog("ERROR: ASI EAF %d EAFGetProperty error %d.", i + 1, result);
                    continue;
                }
                focusers[i] = new ASIEAF(id, info.Name, info.MaxStep);
                iAvailableFocusersCount_ok++;
            }
            IDLog("%d ASI EAF attached out of %d detected.", iAvailableFocusersCount_ok, iAvailableFocusersCount);
            if (iAvailableFocusersCount == iAvailableFocusersCount_ok)
                isInit = true;
        }
    }
}

void ISGetProperties(const char * dev)
{
    ASI_EAF_ISInit();
    for (int i = 0; i < iAvailableFocusersCount; i++)
    {
        ASIEAF * focuser = focusers[i];
        if (dev == nullptr || !strcmp(dev, focuser->m_Name))
        {
            focuser->ISGetProperties(dev);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int num)
{
    ASI_EAF_ISInit();
    for (int i = 0; i < iAvailableFocusersCount; i++)
    {
        ASIEAF * focuser = focusers[i];
        if (dev == nullptr || !strcmp(dev, focuser->m_Name))
        {
            focuser->ISNewSwitch(dev, name, states, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewText(const char * dev, const char * name, char * texts[], char * names[], int num)
{
    ASI_EAF_ISInit();
    for (int i = 0; i < iAvailableFocusersCount; i++)
    {
        ASIEAF * focuser = focusers[i];
        if (dev == nullptr || !strcmp(dev, focuser->m_Name))
        {
            focuser->ISNewText(dev, name, texts, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewNumber(const char * dev, const char * name, double values[], char * names[], int num)
{
    ASI_EAF_ISInit();
    for (int i = 0; i < iAvailableFocusersCount; i++)
    {
        ASIEAF * focuser = focusers[i];
        if (dev == nullptr || !strcmp(dev, focuser->m_Name))
        {
            focuser->ISNewNumber(dev, name, values, names, num);
            if (dev != nullptr)
                break;
        }
    }
}

void ISNewBLOB(const char * dev, const char * name, int sizes[], int blobsizes[], char * blobs[], char * formats[],
               char * names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle * root)
{
    ASI_EAF_ISInit();
    for (int i = 0; i < iAvailableFocusersCount; i++)
    {
        ASIEAF * focuser = focusers[i];
        focuser->ISSnoopDevice(root);
    }
}

ASIEAF::ASIEAF(int id, const char * name, const int maxSteps) : m_ID(id), m_MaxSteps(maxSteps)
{
    // Can move in Absolute & Relative motions, can AbortFocuser motion, and can reverse.
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_REVERSE |
                      FOCUSER_CAN_SYNC);

    // Just USB
    setSupportedConnections(CONNECTION_NONE);

    if (iAvailableFocusersCount > 1)
        snprintf(m_Name, MAXINDINAME, "ASI %s %d", name, id);
    else
        snprintf(m_Name, MAXINDIDEVICE, "ASI %s", name);

    FocusAbsPosN[0].max = maxSteps;
}

bool ASIEAF::initProperties()
{
    INDI::Focuser::initProperties();

    // Focuser temperature
    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%6.2f", -50, 70., 0., 0.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature",
                       MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    FocusRelPosN[0].min   = 0.;
    FocusRelPosN[0].max   = m_MaxSteps / 2.0;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step  = FocusRelPosN[0].max / 20;

    FocusAbsPosN[0].min   = 0.;
    FocusAbsPosN[0].max   = m_MaxSteps;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = m_MaxSteps / 20.0;

    setDefaultPollingPeriod(500);

    addDebugControl();

    return true;
}

bool ASIEAF::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        float temperature = -273;
        EAFGetTemp(m_ID, &temperature);

        if (temperature != -273)
        {
            TemperatureN[0].value = temperature;
            TemperatureNP.s = IPS_OK;
            defineNumber(&TemperatureNP);
        }

        GetFocusParams();

        LOG_INFO("ASI EAF paramaters updated, focuser ready for use.");
    }
    else
    {
        if (TemperatureNP.s != IPS_IDLE)
            deleteProperty(TemperatureNP.name);
    }

    return true;
}

const char * ASIEAF::getDefaultName()
{
    return "ASI EAF";
}

bool ASIEAF::Connect()
{
    EAF_ERROR_CODE rc = EAFOpen(m_ID);

    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to connect to ASI EAF focuser ID: %d (%d)", m_ID, rc);
        return false;
    }

    readMaxPosition();

    return true;
}

bool ASIEAF::Disconnect()
{
    return (EAFClose(m_ID) == EAF_SUCCESS);
}

bool ASIEAF::readTemperature()
{
    float temperature = 0;
    EAF_ERROR_CODE rc = EAFGetTemp(m_ID, &temperature);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read temperature. Error: %d", rc);
        return false;
    }

    TemperatureN[0].value = temperature;
    return true;
}

bool ASIEAF::readPosition()
{
    int step = 0;
    EAF_ERROR_CODE rc = EAFGetPosition(m_ID, &step);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read position. Error: %d", rc);
        return false;
    }
    FocusAbsPosN[0].value = step;
    return true;
}

bool ASIEAF::readMaxPosition()
{
    int max = 0;
    EAF_ERROR_CODE rc = EAFGetMaxStep(m_ID, &max);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read max step. Error: %d", rc);
        return false;
    }
    FocusAbsPosN[0].max = max;
    return true;
}

bool ASIEAF::SetFocuserMaxPosition(uint32_t ticks)
{
    EAF_ERROR_CODE rc = EAFSetMaxStep(m_ID, ticks);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set max step. Error: %d", rc);
        return false;
    }
    return true;
}

bool ASIEAF::readReverse()
{
    bool reversed = false;
    EAF_ERROR_CODE rc = EAFGetReverse(m_ID, &reversed);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read reversed status. Error: %d", rc);
        return false;
    }

    FocusReverseS[REVERSED_ENABLED].s  = reversed ? ISS_ON : ISS_OFF;
    FocusReverseS[REVERSED_DISABLED].s = reversed ? ISS_OFF : ISS_ON;
    FocusReverseSP.s = IPS_OK;
    return true;
}

bool ASIEAF::ReverseFocuser(bool enabled)
{
    EAF_ERROR_CODE rc = EAFSetReverse(m_ID, enabled);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set reversed status. Error: %d", rc);
        return false;
    }

    return true;
}

bool ASIEAF::isMoving()
{
    bool moving = false;
    EAF_ERROR_CODE rc = EAFIsMoving(m_ID, &moving);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to read motion status. Error: %d", rc);
        return false;
    }
    return moving;
}

bool ASIEAF::SyncFocuser(uint32_t ticks)
{
    EAF_ERROR_CODE rc = EAFResetPostion(m_ID, ticks);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to sync focuser. Error: %d", rc);
        return false;
    }
    return true;
}

bool ASIEAF::gotoAbsolute(uint32_t position)
{
    EAF_ERROR_CODE rc = EAFMove(m_ID, position);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to set position. Error: %d", rc);
        return false;
    }
    return true;
}


//bool ASIEAF::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
//{
//    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
//    {
//    }

//    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
//}

//bool ASIEAF::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
//{
//    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
//    {
//    }

//    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
//}

void ASIEAF::GetFocusParams()
{
    if (readPosition())
        IDSetNumber(&FocusAbsPosNP, nullptr);

    if (readReverse())
        IDSetSwitch(&FocusReverseSP, nullptr);
}

IPState ASIEAF::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    if (!gotoAbsolute(targetPos))
        return IPS_ALERT;

    return IPS_BUSY;
}

IPState ASIEAF::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosN[0].value - ticks;
    else
        newPosition = FocusAbsPosN[0].value + ticks;

    // Clamp
    newPosition = std::max(0, std::min(static_cast<int32_t>(FocusAbsPosN[0].max), newPosition));
    if (!gotoAbsolute(newPosition))
        return IPS_ALERT;

    FocusRelPosN[0].value = ticks;
    FocusRelPosNP.s       = IPS_BUSY;

    return IPS_BUSY;
}

void ASIEAF::TimerHit()
{
    if (!isConnected())
    {
        SetTimer(POLLMS);
        return;
    }

    bool rc = readPosition();
    if (rc)
    {
        if (fabs(lastPos - FocusAbsPosN[0].value) > 5)
        {
            IDSetNumber(&FocusAbsPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
        }
    }

    rc = readTemperature();
    if (rc)
    {
        if (fabs(lastTemperature - TemperatureN[0].value) >= 0.1)
        {
            IDSetNumber(&TemperatureNP, nullptr);
            lastTemperature = TemperatureN[0].value;
        }
    }

    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY)
    {
        if (!isMoving())
        {
            FocusAbsPosNP.s = IPS_OK;
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusAbsPosNP, nullptr);
            IDSetNumber(&FocusRelPosNP, nullptr);
            lastPos = FocusAbsPosN[0].value;
            LOG_INFO("Focuser reached requested position.");
        }
    }

    SetTimer(POLLMS);
}

bool ASIEAF::AbortFocuser()
{
    EAF_ERROR_CODE rc = EAFStop(m_ID);
    if (rc != EAF_SUCCESS)
    {
        LOGF_ERROR("Failed to stop focuser. Error: %d", rc);
        return false;
    }
    return true;
}

//bool ASIEAF::saveConfigItems(FILE * fp)
//{
//    Focuser::saveConfigItems(fp);

//    return true;
//}
