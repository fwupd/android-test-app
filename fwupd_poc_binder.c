#include "gbinder_client.h"
#include "gbinder_reader.h"
#include "gbinder_writer.h"
#include <glib.h>
#include <glib-unix.h>
#include <stdio.h>
#include <gbinder.h>
#include <string.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>

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
    GET_INT,
    SET_INT,
    GET_INT_ARRAY,
    //SET_INT_ARRAY,
    ADD_LISTENER,
    REMOVE_LISTENER,
    TRIGGER_CHANGE,
};

enum listener_transactions {
    FWUPD_LISTENER_ON_CHANGED = GBINDER_FIRST_CALL_TRANSACTION,
    FWUPD_LISTENER_ON_DEVICE_ADDED,
};

struct PocDaemon {
    GBinderServiceManager *sm;
    GBinderLocalObject *service_obj;
    GPtrArray* listener_array;
};

struct ClientCallbacks {
    void (*onChanged)();
    void (*onDeviceAdded)();
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
    struct PocDaemon* daemon = user_data;
    GBinderLocalReply *reply = NULL;
    GBinderReader reader;
    gbinder_remote_request_init_reader(req, &reader);
    g_autofree gchar *str2 = g_strdup_printf("here is the message for the client");
    gchar *iface = gbinder_remote_request_interface(req);

    g_message("received %s %d", iface, code);
    if (g_str_equal(iface, "org.freedesktop.fwupd.IPocFwupd")) {
        switch (code) {
            case GET_STRING:
                reply = gbinder_local_object_new_reply(obj);

                gbinder_local_reply_append_int32(reply, GBINDER_STATUS_OK);
                gbinder_local_reply_append_string16(reply, str2);

                *status = GBINDER_STATUS_OK;
                g_debug("get_string \"%s\"", str2);
                return reply;
                break;
            case SET_STRING:
                gchar *str = gbinder_reader_read_string16(&reader);
                g_debug("set_string \"%s\"", str);
                break;
            case GET_FD:
                g_warning("get_fd unimplemented");
                break;
            case SET_FD:
                g_warning("set_fd unimplemented");
                break;
            case GET_DICT:
                g_warning("get_dict unimplemented");
                break;
            case SET_DICT:
                g_warning("set_dict unimplemented");
                break;
            case GET_INT:
                gint32 int_value = 99;
                reply = gbinder_local_object_new_reply(obj);

                gbinder_local_reply_append_int32(reply, GBINDER_STATUS_OK);
                gbinder_local_reply_append_int32(reply, int_value);

                g_debug("get_int \"%d\"", int_value);
                return reply;
                break;
            case SET_INT:
                gint32 int_value2 = 0;
                gbinder_reader_read_int32(&reader, &int_value2);
                g_debug("set_int \"%d\"", int_value2);
                break;
            case ADD_LISTENER:
                GBinderRemoteObject *listener_obj = gbinder_reader_read_object(&reader);
                g_ptr_array_add(daemon->listener_array, listener_obj);
                g_debug("add_listener");
                break;
            case REMOVE_LISTENER:
                g_debug("remove_listener unimplemented");
                break;
            case TRIGGER_CHANGE:
                for (guint i = 0; i < daemon->listener_array->len; i++ )
                {
                    GBinderRemoteObject *listener_obj = g_ptr_array_index(daemon->listener_array, i);
                    GBinderClient *listener_client = gbinder_client_new(listener_obj, "org.freedesktop.fwupd.IFwupdListener");
                    GBinderLocalRequest* listener_req  = gbinder_client_new_request(listener_client);

                    gint listener_req_status;
                    gbinder_client_transact_sync_reply(listener_client, FWUPD_LISTENER_ON_CHANGED /* changed */, listener_req, &listener_req_status);
                }

                g_debug("trigger_change");
                break;
            default:
                g_warning("received unknown code %d", code);
        }
    }
    else {
        g_message("unexpected interface %s", iface);
    }

    return reply;
}

static void
handle_add_service_cb(GBinderServiceManager *sm, int status, gpointer user_data)
{
    //struct PocDaemon* daemon = user_data;

    if (status == GBINDER_STATUS_OK) {
        g_message("service registered: " PROJECT_NAME);
    } else {
        g_warning("failed to register service");
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

gboolean
netlink_cb(gint fd, GIOCondition condition, gpointer user_data)
{
    gssize len;
    guint8 buf[10240] = {0x0};
    g_autoptr(GBytes) blob = NULL;

    len = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
    if (len < 0)
        return G_SOURCE_CONTINUE;

    blob = g_bytes_new(buf, len);
    g_message("netlink: %s", buf);

    return G_SOURCE_CONTINUE;
}

struct PocDaemon*
start_service(void) {
    struct PocDaemon *daemon = NULL;

    GBinderServiceManager *sm = gbinder_servicemanager_new2(DEFAULT_DEVICE, RPC_PROTO, RPC_PROTO);
    // TODO: Using the android 14 patch for libgbinder should allow this to work however whilst it doesn't report failing, it isn't listed by clients.
    //GBinderServiceManager *sm = gbinder_servicemanager_new2(DEFAULT_DEVICE, NULL, NULL);
    if (sm == NULL) {
        g_warning("failed to create service manager with version " RPC_PROTO);
        return NULL;
    }

    if (!gbinder_servicemanager_wait(sm, -1)) {
        g_warning("failed to wait for service manager");
        return NULL;
    }

    daemon = calloc(1, sizeof(*daemon));
    daemon->listener_array = g_ptr_array_new();
    daemon->sm = sm;

    // TODO: Many services list "null" as their interface
    // Service is listed as android.hidl.base@1.0::IBase if iface is NULL, and the string if it is not
    GBinderLocalObject *poc_service_obj = gbinder_servicemanager_new_local_object(sm, "org.freedesktop.fwupd.IPocFwupd", handle_calls_cb, daemon);

    daemon->service_obj = poc_service_obj;

    gbinder_servicemanager_add_presence_handler(sm, handle_presence_cb, daemon);

    gbinder_servicemanager_add_service(sm, PROJECT_NAME, poc_service_obj, handle_add_service_cb, daemon);

    return daemon;
}

void
start_netlink(void)
{
    struct sockaddr_nl nls = {
        .nl_family = AF_NETLINK,
        .nl_pid = getpid(),
        .nl_groups = RTMGRP_LINK,
    };
    g_autoptr(GSource) source = NULL;

    gint netlink_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
    if (netlink_fd < 0) {
        g_warning("netlink socket fail %s", g_strerror(errno));
        return;
    }

    if (bind(netlink_fd, (void *)&nls, sizeof(nls))) {
        g_warning("failed to bind udev socket %s", g_strerror(errno));
        return;
    }

    g_message("connected to netlink");

    source = g_unix_fd_source_new(netlink_fd, G_IO_IN);
    g_source_set_callback(source, G_SOURCE_FUNC(netlink_cb), &netlink_fd, NULL);

    g_source_attach(source, NULL);
}

int main(void) {
    GMainLoop *loop = g_main_loop_new(NULL, TRUE);

    struct PocDaemon* daemon = start_service();

    start_netlink();

    // loop
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    free(daemon);

    return EXIT_SUCCESS;
}
