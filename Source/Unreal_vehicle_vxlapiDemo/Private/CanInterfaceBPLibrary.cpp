// Fill out your copyright notice in the Description page of Project Settings.


#include "CanInterfaceBPLibrary.h"

#if defined(_Windows) || defined(_MSC_VER) || defined (__GNUC__)
 #define  STRICT
 #include <windows.h>
#endif

#include <stdio.h>
#include <queue>


THIRD_PARTY_INCLUDES_START
#include "../ThirdParty/vxlapi.h"
THIRD_PARTY_INCLUDES_END

#define UNUSED_PARAM(a) { a=a; }

#define RECEIVE_EVENT_SIZE         1        // DO NOT EDIT! Currently 1 is supported only
#define RX_QUEUE_SIZE              4096     // internal driver queue size in CAN events
#define RX_QUEUE_SIZE_FD           16384    // driver queue size for CAN-FD Rx events
#define ENABLE_CAN_FD_MODE_NO_ISO  0        // switch to activate no iso mode on a CAN FD channel

#define CANIF_BPLIB_MAX_RX_QUEUE_SIZE 10u //RX FIFO size, if it is full, then more rx events will be skipped in the RX thread

/////////////////////////////////////////////////////////////////////////////
// globals

char            g_AppName[XL_MAX_APPNAME+1]  = "Unreal_vxlapi";               //!< Application name which is displayed in VHWconf
XLportHandle    g_xlPortHandle              = XL_INVALID_PORTHANDLE;      //!< Global porthandle (we use only one!)
XLdriverConfig  g_xlDrvConfig;                                            //!< Contains the actual hardware configuration
XLaccess        g_xlChannelMask             = 0;                          //!< Global channelmask (includes all founded channels)
XLaccess        g_xlPermissionMask          = 0;                          //!< Global permissionmask (includes all founded channels)
unsigned int    g_BaudRate                  = 500000;                     //!< Default baudrate
int             g_silent                    = 0;                          //!< flag to visualize the message events (on/off)
unsigned int    g_TimerRate                 = 0;                          //!< Global timerrate (to toggel)
unsigned int    g_canFdSupport              = 0;                          //!< Global CAN FD support flag
unsigned int    g_canFdModeNoIso            = ENABLE_CAN_FD_MODE_NO_ISO;  //!< Global CAN FD ISO (default) / no ISO mode flag

XLaccess        g_xlChanMaskTx = 0;
unsigned int    g_xlChanIndex = 0;

// tread variables
XLhandle        g_hMsgEvent;                                          //!< notification handle for the receive queue
HANDLE          g_hRXThread;                                          //!< thread handle (RX)
HANDLE          g_hTXThread;                                          //!< thread handle (TX)
int             g_RXThreadRun;                                        //!< flag to start/stop the RX thread
int             g_TXThreadRun;                                        //!< flag to start/stop the TX thread (for the transmission burst)
int             g_RXCANThreadRun;                                     //!< flag to start/stop the RX thread
unsigned int    g_TXThreadCanId ;                                     //!< CAN-ID the TX thread transmits under
XLaccess        g_TXThreadTxMask;                                     //!< channel mask the TX thread uses for transmitting


std::queue<XLevent> g_xlEvent_queue; //!< rx message queue (FIFO)



////////////////////////////////////////////////////////////////////////////

//! demoInitDriver

//! initializes the driver with one port and all founded channels which
//! have a connected CAN cab/piggy.
//!
////////////////////////////////////////////////////////////////////////////

XLstatus demoInitDriver(XLaccess *pxlChannelMaskTx, unsigned int *pxlChannelIndex) {

  XLstatus          xlStatus;
  unsigned int      i;
  XLaccess          xlChannelMaskFd = 0;
  XLaccess          xlChannelMaskFdNoIso = 0;
  auto messageLog = FMessageLog("CanInfterfaceBPLibrary log");
	//messageLog.Open(EMessageSeverity::Info, true);

  // ------------------------------------
  // open the driver
  // ------------------------------------
  xlStatus = xlOpenDriver ();
  
  // ------------------------------------
  // get/print the hardware configuration
  // ------------------------------------
  if(XL_SUCCESS == xlStatus) {
    xlStatus = xlGetDriverConfig(&g_xlDrvConfig);
  }
  
  if(XL_SUCCESS == xlStatus) {    
    // ------------------------------------
    // select the wanted channels
    // ------------------------------------
    g_xlChannelMask = 0;
    for (i=0; i < g_xlDrvConfig.channelCount; i++) {
      
      // we take all hardware we found and supports CAN
      if (g_xlDrvConfig.channel[i].channelBusCapabilities & XL_BUS_ACTIVE_CAP_CAN) { 
        
        if (!*pxlChannelMaskTx) {
          *pxlChannelMaskTx = g_xlDrvConfig.channel[i].channelMask;
          *pxlChannelIndex  = g_xlDrvConfig.channel[i].channelIndex;
        }

        // check if we can use CAN FD - the virtual CAN driver supports CAN-FD, but we don't use it
        if ((g_xlDrvConfig.channel[i].channelCapabilities & XL_CHANNEL_FLAG_CANFD_ISO_SUPPORT)
           && (g_xlDrvConfig.channel[i].hwType != XL_HWTYPE_VIRTUAL)) {
          xlChannelMaskFd |= g_xlDrvConfig.channel[i].channelMask;
          
          // check CAN FD NO ISO support
          if (g_xlDrvConfig.channel[i].channelCapabilities & XL_CHANNEL_FLAG_CANFD_BOSCH_SUPPORT) {
            xlChannelMaskFdNoIso |= g_xlDrvConfig.channel[i].channelMask;
          }
        }
        else {
          g_xlChannelMask |= g_xlDrvConfig.channel[i].channelMask;
        }
        
      }
    }

    // if we found a CAN FD supported channel - we use it.
    if (xlChannelMaskFd && !g_canFdModeNoIso) {
      g_xlChannelMask = xlChannelMaskFd;
      //printf("- Use CAN-FD for   : CM=0x%I64x\n", g_xlChannelMask);
      g_canFdSupport = 1;
    }

    if (xlChannelMaskFdNoIso && g_canFdModeNoIso) {
      g_xlChannelMask = xlChannelMaskFdNoIso;
      //printf("- Use CAN-FD NO ISO for   : CM=0x%I64x\n", g_xlChannelMask);
      g_canFdSupport = 1;
    }
    
    if (!g_xlChannelMask) {
      //printf("ERROR: no available channels found! (e.g. no CANcabs...)\n\n");
      xlStatus = XL_ERROR;
    }
  }

  g_xlPermissionMask = g_xlChannelMask;
  
  // ------------------------------------
  // open ONE port including all channels
  // ------------------------------------
  if(XL_SUCCESS == xlStatus) {
    
    // check if we can use CAN FD
    if (g_canFdSupport) {
      xlStatus = xlOpenPort(&g_xlPortHandle, g_AppName, g_xlChannelMask, &g_xlPermissionMask, RX_QUEUE_SIZE_FD, XL_INTERFACE_VERSION_V4, XL_BUS_TYPE_CAN);
    }
    // if not, we make 'normal' CAN
    else {
      xlStatus = xlOpenPort(&g_xlPortHandle, g_AppName, g_xlChannelMask, &g_xlPermissionMask, RX_QUEUE_SIZE, XL_INTERFACE_VERSION, XL_BUS_TYPE_CAN);
     
    }
     //printf("- OpenPort         : CM=0x%I64x, PH=0x%02X, PM=0x%I64x, %s\n", 
     //         g_xlChannelMask, g_xlPortHandle, g_xlPermissionMask, xlGetErrorString(xlStatus));
       
  }

  if ( (XL_SUCCESS == xlStatus) && (XL_INVALID_PORTHANDLE != g_xlPortHandle) ) {
    
    // ------------------------------------
    // if we have permission we set the
    // bus parameters (baudrate)
    // ------------------------------------
    if (g_xlChannelMask == g_xlPermissionMask) {

      if(g_canFdSupport) {
        XLcanFdConf fdParams;
       
        memset(&fdParams, 0, sizeof(fdParams));
        
        // arbitration bitrate
        fdParams.arbitrationBitRate = 1000000;
        fdParams.tseg1Abr           = 6;
        fdParams.tseg2Abr           = 3;
        fdParams.sjwAbr             = 2;

        // data bitrate
        fdParams.dataBitRate = fdParams.arbitrationBitRate*2;
        fdParams.tseg1Dbr    = 6;
        fdParams.tseg2Dbr    = 3;
        fdParams.sjwDbr      = 2;

        if (g_canFdModeNoIso) {
          fdParams.options = CANFD_CONFOPT_NO_ISO;
        }

        xlStatus = xlCanFdSetConfiguration(g_xlPortHandle, g_xlChannelMask, &fdParams);
        //printf("- SetFdConfig.     : ABaudr.=%u, DBaudr.=%u, %s\n", fdParams.arbitrationBitRate, fdParams.dataBitRate, xlGetErrorString(xlStatus));

      }
      else {
        xlStatus = xlCanSetChannelBitrate(g_xlPortHandle, g_xlChannelMask, g_BaudRate);
        //printf("- SetChannelBitrate: baudr.=%u, %s\n",g_BaudRate, xlGetErrorString(xlStatus));
      }
    } 
    else {
      //printf("-                  : we have NO init access!\n");
    }
  }  
  else
  {

    xlClosePort(g_xlPortHandle);
    g_xlPortHandle = XL_INVALID_PORTHANDLE;
    xlStatus = XL_ERROR;
  }

  return xlStatus;
  
}                    

///////////////////////////////////////////////////////////////////////////

//! RxThread

//! thread to readout the message queue and parse the incoming messages
//!
////////////////////////////////////////////////////////////////////////////

DWORD WINAPI RxThread(LPVOID par) 
{
  XLstatus        xlStatus;
  
  unsigned int    msgsrx = RECEIVE_EVENT_SIZE;
  XLevent         xlEvent; 
  
  UNUSED_PARAM(par); 
  
  g_RXThreadRun = 1;

  while (g_RXThreadRun) { 
   
    WaitForSingleObject(g_hMsgEvent,10);

    xlStatus = XL_SUCCESS;
    
    while (!xlStatus) {
      
      msgsrx = RECEIVE_EVENT_SIZE;

      xlStatus = xlReceive(g_xlPortHandle, &msgsrx, &xlEvent);      
      if ( xlStatus!=XL_ERR_QUEUE_IS_EMPTY ) {

        if (!g_silent) {                    
          //Add rx event to FIFO
          g_xlEvent_queue.push(xlEvent);

          //Throw away first element in case of buffer overrun
          if (g_xlEvent_queue.size() > CANIF_BPLIB_MAX_RX_QUEUE_SIZE)
          {
            g_xlEvent_queue.pop();
          }
        }
      }  
    } 
  }
  return NO_ERROR; 
}

////////////////////////////////////////////////////////////////////////////

//! demoCreateRxThread

//! set the notification and creates the thread.
//!
////////////////////////////////////////////////////////////////////////////

XLstatus demoCreateRxThread(void) {
  XLstatus      xlStatus = XL_ERROR;
  DWORD         ThreadId=0;
 
  if (g_xlPortHandle!= XL_INVALID_PORTHANDLE) {

      // Send a event for each Msg!!!
      xlStatus = xlSetNotification (g_xlPortHandle, &g_hMsgEvent, 1);

      if (! g_canFdSupport) {
        g_hRXThread = CreateThread(0, 0x2000, RxThread, (LPVOID) 0, 0, &ThreadId);
      }

  }
  return xlStatus;
}

bool UCanInterfaceBPLibrary::vxlapiInit()
{
  XLstatus          xlStatus;  
  XLaccess          xlChannelMaskFd = 0;  
  XLaccess          xlChannelMaskFdNoIso = 0;
  //auto messageLog = FMessageLog("CanInfterfaceBPLibrary log");
	//messageLog.Open(EMessageSeverity::Info, true);
	
  // ------------------------------------
  // open the driver
  // ------------------------------------
  xlStatus = demoInitDriver(&g_xlChanMaskTx, &g_xlChanIndex);

  if(XL_SUCCESS == xlStatus) {
    // ------------------------------------
    // create the RX thread to read the
    // messages
    // ------------------------------------
    xlStatus = demoCreateRxThread();
  }

  if (XL_SUCCESS == xlStatus)
  {
    //messageLog.Message(EMessageSeverity::Info, FText::FromString("demoInitDriver() succeeded!"));
  }
  else
  {
    //messageLog.Message(EMessageSeverity::Warning, FText::FromString("Error: demoInitDriver()!!!"));
  }


  if(XL_SUCCESS == xlStatus) {
    // ------------------------------------
    // go with all selected channels on bus
    // ------------------------------------
    xlStatus = xlActivateChannel(g_xlPortHandle, g_xlChannelMask, XL_BUS_TYPE_CAN, XL_ACTIVATE_RESET_CLOCK);
  
  }

  if (XL_SUCCESS == xlStatus)
  {
    return true;
  }
  else
  {
    return false;
  }
  
}

bool UCanInterfaceBPLibrary::vxlapiDeInit()
{
  if (XL_INVALID_PORTHANDLE != g_xlPortHandle)
  {
    g_RXThreadRun = false;
    xlClosePort(g_xlPortHandle);
    g_xlPortHandle = XL_INVALID_PORTHANDLE;
    
    while (g_xlEvent_queue.size())
    {
      g_xlEvent_queue.pop();
    }

        
    return true;
  }
  else
  {
    return false;
  }
}


bool UCanInterfaceBPLibrary::vxlapiSendCanMessage(int canId, TArray<uint8> data, int len)
{
  //auto messageLog = FMessageLog("CanInfterfaceBPLibrary log");
  bool ret_val = false;
  XLstatus             xlStatus;
  static XLevent       xlEvent;
  unsigned int         messageCount = 1;
  uint8 idx;
  if (XL_INVALID_PORTHANDLE != g_xlPortHandle)
  {
    //Only normal CAN supported currently, no CAN FD support
    if(! g_canFdSupport)
    {      
      memset(&xlEvent, 0, sizeof(xlEvent));
      xlEvent.tag                 = XL_TRANSMIT_MSG;
      xlEvent.tagData.msg.id      = canId;
      xlEvent.tagData.msg.dlc = len;
      for (idx = 0; idx < len; idx++)
      {
        xlEvent.tagData.msg.data[idx] = data[idx];
      }
      
      xlStatus = xlCanTransmit(g_xlPortHandle, g_xlChanMaskTx, &messageCount, &xlEvent);
      if (XL_SUCCESS == xlStatus)
      {
        //messageLog.Message(EMessageSeverity::Info, FText::FromString("Successful: vxlapiSendCanMessage()!"));
        ret_val = true;
      }
      else
      {
        //messageLog.Message(EMessageSeverity::Info, FText::FromString("Error: vxlapiSendCanMessage()!"));
      }
    }
  }
  
  return ret_val;
    
}

bool UCanInterfaceBPLibrary::vxlapiPollCanRxMessageQueue(UPARAM(ref) int &out_canId, UPARAM(ref) TArray<uint8> &out_data, UPARAM(ref) int &out_len)
{
  XLevent xlEvent;
  bool ret_val = false;
  int idx;
  if (g_xlEvent_queue.size())
  {
    xlEvent = g_xlEvent_queue.front();
    g_xlEvent_queue.pop();
    
    out_canId = xlEvent.tagData.msg.id;
    out_len = xlEvent.tagData.msg.dlc;
    for (idx = 0; idx < out_len; idx++)
    {
      out_data[idx] = xlEvent.tagData.msg.data[idx];
    }
    ret_val = true;
  }
  
  return ret_val;
}
