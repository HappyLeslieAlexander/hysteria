#include "hysteria_mod.h"

struct hysteria_stats g_hysteria_stats = {0};
int g_hysteria_mode = 1;

static int
hysteria_modevent(module_t mod, int event, void *arg)
{
    (void)mod;
    (void)arg;

    switch (event) {
    case MOD_LOAD:
        if (hysteria_ctl_init() != 0)
            return (EIO);
        if (hysteria_net_init() != 0) {
            hysteria_ctl_fini();
            return (EIO);
        }
        uprintf("hysteria: module loaded\n");
        return (0);
    case MOD_UNLOAD:
        hysteria_net_fini();
        hysteria_ctl_fini();
        uprintf("hysteria: module unloaded\n");
        return (0);
    default:
        return (EOPNOTSUPP);
    }
}

static moduledata_t hysteria_mod = {
    "hysteria",
    hysteria_modevent,
    NULL
};

DECLARE_MODULE(hysteria, hysteria_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(hysteria, 1);
