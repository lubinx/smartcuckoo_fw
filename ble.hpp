#ifndef __BLE_HPP
#define __BLE_HPP                       1

#include "smartcuckoo.h"

#include <bluetooth/gap.tlv.hpp>
#include <bluetooth/gatt.service.serialp.hpp>

#include "nations.bluetooth.hpp"

    class TUltraCorePeripheral :public Bluetooth::Nations::TPeripheral
    {
        typedef Bluetooth::Nations::TPeripheral inherited;

    public:
        TUltraCorePeripheral() :
            inherited(PROJECT_NAME),
            Shell(), PeerId(0)
        {
        }

        // virtual void GAP_Dispatch(uint32_t const msg_id, void *arg) override
        // {
        //     inherited::GAP_Dispatch(msg_id, arg);

        //     if (PERIPHERIAL_CONN_TIMEOUT && PeerId && clock() - LastActivity > PERIPHERIAL_CONN_TIMEOUT)
        //     {
        //         Connections.Close(PeerId);
        //         PeerId = 0;
        //     }
        // }

        virtual void GAP_OnReady(void) override
        {
            inherited::GAP_OnReady();
            PERIPHERAL_on_ready();
        }

        virtual void CLI_OnConnected(uint16_t peer_id, void *arg) override
        {
            inherited::CLI_OnConnected(peer_id, arg);

            PeerId = peer_id;
            PERIPHERAL_on_connected();
        }

        virtual void CLI_OnDisconnect(uint16_t peer_id, void *arg) override
        {
            inherited::CLI_OnDisconnect(peer_id, arg);

            PeerId = peer_id;
            PERIPHERAL_on_disconnect();
            ADV_Start();
        }

        virtual void GAP_OnSleep() override
        {
            inherited::GAP_OnSleep();
            PERIPHERAL_on_sleep();
        }

        virtual void GAP_OnSleepWakeup() override
        {
            inherited::GAP_OnSleepWakeup();
            ADV_Update();
            PERIPHERAL_on_wakeup();
        }

        virtual void ADV_GetScanResponseData(Bluetooth::TAdvStream &scanrsp) override
        {
            ADV_WriteManufacturerData(scanrsp,
                FBDAddr, PROJECT_ID, PROJECT_VERSION,
                BATT_mv_level(PERIPHERAL_batt_volt()),
                PERIPHERAL_sync_id()
            );

            PERIPHERAL_write_tlv(scanrsp);
            ADV_Finalize(scanrsp);
        }

        Bluetooth::TSerialPortService Shell;

    private:
        uint16_t PeerId;
    };
    extern TUltraCorePeripheral BLE;

static inline __attribute__((nothrow, nonnull))
    void BLE_SHELL_write(char const *str)
    {
        BLE.Shell.Write(str, strlen(str));
    }

#endif
