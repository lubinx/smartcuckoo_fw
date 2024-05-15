#ifndef __SMARTCUCKOO_IRDA_H
#define __SMARTCUCKOO_IRDA_H

#include <features.h>

#include <stdbool.h>
#include <stdint.h>

    struct IrDA_t
    {
        uint32_t value;
    };


    // https://www.sbprojects.net/knowledge/ir/nec.php
    struct IrDA_nec_t
    {
        union
        {
            uint16_t addr_ext;

            struct 
            {
                uint8_t addr;
                uint8_t addr_inv;
            };
        };

        struct
        {
            uint8_t cmd;
            uint8_t cmd_inv;
        };
    };

    typedef void (*IrDA_callback_t)(struct IrDA_t *irda, bool repeat, void *arg);

    struct IrDA_attr_t
    {
        int idx;
        void *dev;
        uint32_t tick;

        IrDA_callback_t callback;
        void *arg;

        struct IrDA_t da;
    };

__BEGIN_DECLS

extern __attribute__((nothrow))
    void IrDA_init(struct IrDA_attr_t *attr, uint32_t PIN, IrDA_callback_t callback, void *arg);

__END_DECLS
#endif
