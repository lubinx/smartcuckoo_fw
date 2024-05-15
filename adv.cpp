#include <time.h>
#include "ble.hpp"

void PERIPHERAL_write_tlv(TMemStream &scanrsp)
{
    time_t ts = time(NULL);

    BluetoothTLV tlv(ATT_UNIT_UNITLESS, 1, (uint32_t)ts);
    scanrsp.Write(&tlv, tlv.size());
}
