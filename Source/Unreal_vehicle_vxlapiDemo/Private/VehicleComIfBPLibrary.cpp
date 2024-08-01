// Fill out your copyright notice in the Description page of Project Settings.


#include "VehicleComIfBPLibrary.h"
#include "CanInterfaceBPLibrary.h"

#if defined(_Windows) || defined(_MSC_VER) || defined (__GNUC__)
 #define  STRICT
 #include <windows.h>
#endif
#include <chrono>
#include <thread>
#include <algorithm>

using namespace std::chrono_literals;

//Globals for thread management and state handling
HANDLE          g_hThread;
static bool g_DeinitRequest = false;

static float _Velocity;


static void ComIfLib_TransmitCarControlHmiMessage(void)
{    
    
    TArray<uint8> payload;
    payload.Init(0,8);
    
    payload[0] = (byte)_Velocity;
    
    UCanInterfaceBPLibrary::vxlapiSendCanMessage(0x1, payload, 1);
}

static void ComIfLib_ReceiveCanMessages(void)
{
    int rx_can_id;
    int rx_payload_len;
    TArray<uint8> rx_payload;
    rx_payload.Init(0,8);

    bool queue_empty = false;
    uint8_t idx_max_poll = 10;
    do
    {
        idx_max_poll--;
        if (UCanInterfaceBPLibrary::vxlapiPollCanRxMessageQueue(rx_can_id, rx_payload, rx_payload_len))
        {
            //To Do: Story payload in buffers and read from them in provided blueprint interface functions

        }
        else
        {
            queue_empty = true;
        }                
    } while ((! queue_empty) && idx_max_poll);
    
}

///////////////////////////////////////////////////////////////////////////

//! ComIfLib_Thread

//! thread to realize a background task to handle cyclic processed Can Transmit
//!
////////////////////////////////////////////////////////////////////////////

DWORD WINAPI ComIfLib_Thread(LPVOID par)  
{
  par = par;
  static int idx = 0;
  
  while(! g_DeinitRequest)
  {
    std::this_thread::sleep_for(1000ms);    
    
    //Sub function to receive CAN messages and to decode receive signals to global buffers for these interfaces
    ComIfLib_ReceiveCanMessages();
    ComIfLib_TransmitCarControlHmiMessage();
  }
  UCanInterfaceBPLibrary::vxlapiDeInit();  
  return NO_ERROR;   
}

void UVehicleComIfBPLibrary::InitTask()
{
    DWORD         ThreadId=0;
    UCanInterfaceBPLibrary::vxlapiInit();
    g_DeinitRequest = false;
    
    g_hThread = CreateThread(0, 0x2000, ComIfLib_Thread, (LPVOID) 0, 0, &ThreadId);
}

void UVehicleComIfBPLibrary::DeInitTask()
{    
    g_DeinitRequest = true;
    UCanInterfaceBPLibrary::vxlapiDeInit();     
}

void UVehicleComIfBPLibrary::SendVelocity(float in_Velocity)
{      
    /*
    * Convert velocity unit from chaos vehicle component to km/h
    * Chaos vehicle component works with cm/s
    */
    _Velocity = in_Velocity * 0.036;    
}



