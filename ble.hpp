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
            Shell()
        {
            #ifdef NDEBUG
                Connections.SetInactiveTimeout(15000);
            #endif
        }

        virtual void GAP_OnReady(void) override
        {
            inherited::GAP_OnReady();
            PERIPHERAL_on_ready();
        }

        virtual void CLI_OnConnected(uint16_t peer_id, void *arg) override
        {
            inherited::CLI_OnConnected(peer_id, arg);
            PERIPHERAL_on_connected();
        }

        virtual void CLI_OnDisconnect(uint16_t peer_id, void *arg) override
        {
            inherited::CLI_OnDisconnect(peer_id, arg);
            PERIPHERAL_on_disconnect();
            ADV_Start();
        }

        virtual void CLI_OnSubscribe(Bluetooth::TBaseChar *Char) override
        {
            if (Char == &Shell.Char)
            {
                struct UCSH_env *env = (struct UCSH_env *)malloc(sizeof(struct UCSH_env) + 2048);

                UCSH_init_instance(env, Shell.CreateVFd(), 2048, (uint32_t *)(env + 1));
                UCSH_set_destructor(env, UCSH_destruct);
                LOG_debug("BLE: creating shell 0x%08x", env);
            }
        }

        virtual void GAP_OnSleep() override
        {
            inherited::GAP_OnSleep();
            PERIPHERAL_on_sleep();
        }

        virtual void GAP_OnSleepWakeup() override
        {
            PERIPHERAL_on_wakeup();
            ADV_Update();

            inherited::GAP_OnSleepWakeup();
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
        static void UCSH_destruct(struct UCSH_env *env)
        {
            free(env);
            LOG_debug("BLE: shell destruct");
        }
    };
    // extern TUltraCorePeripheral BLE;

#endif
