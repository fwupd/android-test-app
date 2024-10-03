#include <glib.h>
#include <stdio.h>
#include <gbinder.h>
#include <string.h>

#define DEFAULT_DEVICE "/dev/binder"
#define RPC_PROTO "aidl3"

#define BINDER_TRANSACTION(c2, c3, c4) GBINDER_FOURCC('_', c2, c3, c4)
#define BINDER_INTERFACE_TRANSACTION   BINDER_TRANSACTION('N', 'T', 'F')

void print_service_list(GBinderServiceManager *sm)
{
    char ** service_name_list = gbinder_servicemanager_list_sync(sm);
    char ** ptr;
    for (ptr = service_name_list; *ptr; ptr++)
    {
        
        const char* service_name = *ptr;
        GBinderRemoteObject *remote = gbinder_servicemanager_get_service_sync(sm, service_name, 0);
        GBinderClient *client = gbinder_client_new(remote, NULL);
        int stat;
        GBinderRemoteReply *reply = gbinder_client_transact_sync_reply(client, BINDER_INTERFACE_TRANSACTION, NULL, &stat);

        g_assert(stat == GBINDER_STATUS_OK);

        GBinderReader reader;
        gbinder_remote_reply_init_reader(reply, &reader);

        char* service_interface = gbinder_reader_read_string16(&reader);
        g_print("%s [%s]\n", service_name, service_interface);
    }
}

int main(void) {
    GBinderServiceManager *sm = gbinder_servicemanager_new2(DEFAULT_DEVICE, RPC_PROTO, RPC_PROTO);
    if (sm == NULL) {
        g_error("failed to create service manager with version " RPC_PROTO);
    }

    if (!gbinder_servicemanager_wait(sm, -1)) {
        g_error("failed to wait for service manager");
    }

    print_service_list(sm);

    return 0;
}
