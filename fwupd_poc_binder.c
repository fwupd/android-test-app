#include <glib.h>
#include <stdio.h>
#include <gbinder.h>
#include <string.h>

#define DEFAULT_DEVICE "/dev/binder"
#define RPC_PROTO "aidl3"


#define PROJECT_NAME "fwupd_poc"
#define PROJECT_IFACE "org.freedesktop.fwupd_poc"

#define BINDER_TRANSACTION(c2, c3, c4) GBINDER_FOURCC('_', c2, c3, c4)
#define BINDER_INTERFACE_TRANSACTION   BINDER_TRANSACTION('N', 'T', 'F')

enum poc_transactions {
    GET_STRING = GBINDER_FIRST_CALL_TRANSACTION,
    SET_STRING,
    GET_FD,
    SET_FD,
    GET_DICT,
    SET_DICT,
};

struct PocDaemon {
    GBinderServiceManager *sm;
    GBinderLocalObject *service_obj;
};

static GBinderLocalReply*
handle_calls_cb(
        GBinderLocalObject* obj,
        GBinderRemoteRequest* req,
        guint code,
        guint flags,
        int* status,
        gpointer user_data)
{
    //struct PocDaemon* daemon = user_data;

    g_message("received %d", code);

    const gchar *iface = gbinder_remote_request_interface(req);

    g_message("received %s %d", iface, code);

    return NULL;
}

static void
handle_add_service_cb(GBinderServiceManager *sm, int status, gpointer user_data)
{
    //struct PocDaemon* daemon = user_data;

    if (status == GBINDER_STATUS_OK) {
        g_message("service registered: " PROJECT_NAME);
    } else {
        g_error("failed to register service");
    }

}

static void
handle_presence_cb(GBinderServiceManager *sm, gpointer user_data)
{
    struct PocDaemon* daemon = user_data;
    if (gbinder_servicemanager_is_present(sm))
    {
        gbinder_servicemanager_add_service(sm, PROJECT_NAME, daemon->service_obj, handle_add_service_cb, daemon);
    }
}


int main(void) {
    GMainLoop *loop = g_main_loop_new(NULL, TRUE);

    GBinderServiceManager *sm = gbinder_servicemanager_new2(DEFAULT_DEVICE, RPC_PROTO, RPC_PROTO);
    // TODO: Using the android 14 patch for libgbinder should allow this to work however whilst it doesn't report failing, it isn't listed by clients.
    //GBinderServiceManager *sm = gbinder_servicemanager_new2(DEFAULT_DEVICE, NULL, NULL);
    if (sm == NULL) {
        g_error("failed to create service manager with version " RPC_PROTO);
    }

    if (!gbinder_servicemanager_wait(sm, -1)) {
        g_error("failed to wait for service manager");
    }

    struct PocDaemon daemon;
    daemon.sm = sm;

    // TODO: Many services list "null" as their interface
    // Service is listed as android.hidl.base@1.0::IBase if iface is NULL, and the string if it is not
    GBinderLocalObject *poc_service_obj = gbinder_servicemanager_new_local_object(sm, "org.freedesktop.fwupd.IPocFwupd", handle_calls_cb, &daemon);

    daemon.service_obj = poc_service_obj;

    gbinder_servicemanager_add_presence_handler(sm, handle_presence_cb, &daemon);

    gbinder_servicemanager_add_service(sm, PROJECT_NAME, poc_service_obj, handle_add_service_cb, &daemon);

    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    return 0;
}
