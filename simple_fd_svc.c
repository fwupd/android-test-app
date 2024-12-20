
#include <glib.h>
#include <stdlib.h>

//#if HAS_BINDER_NDK
#include <android/binder_status.h>
#include <android/binder_parcel.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_ibinder.h>
#include <android/persistable_bundle.h>
#include <unistd.h>
//#endif /* HAS_BINDER_NDK */

#define INTERFACE_DESCRIPTOR "org.freedesktop.fwupdpoc.ISimpleFd"
#define SERVICE_NAME "simple_fd"

typedef struct Daemon {
    int binder_fd;
} Daemon;

static int
poll_binder_process(void *user_data) {
    Daemon *daemon = user_data;
    binder_status_t nstatus = STATUS_OK;
    if (daemon->binder_fd < 0)
        return G_SOURCE_CONTINUE;

    nstatus = ABinderProcess_handlePolledCommands();
    if (nstatus != STATUS_OK) {
        AStatus *status = AStatus_fromStatus(nstatus);
        g_warning("failed to handle polled commands %s", AStatus_getDescription(status));
    }

    return G_SOURCE_CONTINUE;
}

static void*
binder_class_on_create(void* user_data)
{
    Daemon *daemon = calloc(1, sizeof(Daemon));
    return daemon;
}

static void
binder_class_on_destroy(void *user_data)
{
    Daemon *daemon = user_data;
    free(daemon);
}


static bool
parcel_string_allocator(void *stringData, int32_t length, char **buffer)
{
	char **outString = (char **)stringData;
	g_warning("string length is %d", length);

	if (length == 0)
		return false;

	if (length == -1) {
		*outString = NULL;
		return true;
	}

	*buffer = calloc(1, length);
	*outString = *buffer;

	return true;
}

static int
method_send_fd(AIBinder *binder, transaction_code_t code, const AParcel *in, AParcel *out)
{
    int fd = 0;
    binder_status_t nstatus = STATUS_OK;

    const char *str = NULL;
    nstatus = AParcel_readString(in, &str, parcel_string_allocator);
    if (nstatus != STATUS_OK) {
        AStatus *status = AStatus_fromStatus(nstatus);
        g_warning("read parcel string status is %s", AStatus_getDescription(status));
    }

    g_warning("string is %s", str);

    nstatus = AParcel_readParcelFileDescriptor(in, &fd);
    if (nstatus != STATUS_OK) {
        AStatus *status = AStatus_fromStatus(nstatus);
        g_warning("read parcel fd status is %s", AStatus_getDescription(status));
    }
    g_warning("fd is %d", fd);

    if (fd > 0) {
        off_t fsize = lseek(fd, 0, SEEK_END);

        g_warning("fd size is %ld", fsize);
    }
    int bundle_null_flag = 0;
    AParcel_readInt32(in, &bundle_null_flag);

    if (bundle_null_flag) {
        APersistableBundle *bundle = APersistableBundle_new();
        nstatus = APersistableBundle_readFromParcel(in, &bundle);
        if (nstatus != STATUS_OK) {
            AStatus *status = AStatus_fromStatus(nstatus);
            g_warning("read parcel bundle status is %s", AStatus_getDescription(status));
        }

        g_warning("bundle size is %d", APersistableBundle_size(bundle));
    }

    return STATUS_OK;
}

static int
binder_class_on_transact(AIBinder *binder, transaction_code_t code, const AParcel *in, AParcel *out)
{
    g_warning("transaction is %u", code);
    static const AIBinder_Class_onTransact method_funcs[] = {
        NULL,
        method_send_fd,
    };

    if (code < 0 && code > G_N_ELEMENTS(method_funcs)) {
        return STATUS_INVALID_OPERATION;
    }

    return method_funcs[code](binder, code, in, out);
}

int main(void) {
    int binder_fd = 0;
    g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);
    binder_status_t nstatus = STATUS_OK;

    AIBinder_Class *binder_class = AIBinder_Class_define(
        INTERFACE_DESCRIPTOR,
        binder_class_on_create,
        binder_class_on_destroy,
        binder_class_on_transact);

    AIBinder *binder = AIBinder_new(binder_class, NULL);

    Daemon *daemon = AIBinder_getUserData(binder);

    g_warning("add service");
    nstatus = AServiceManager_addService(binder, SERVICE_NAME);
    if (nstatus != STATUS_OK) {
        AStatus *status = AStatus_fromStatus(nstatus);
        g_warning("add service status %s", AStatus_getDescription(status));
    }

    AIBinder *binder_check = AServiceManager_checkService(SERVICE_NAME);
    g_warning("service check %p", (void*)binder_check);

    //TODO: Is this more appropriate for fwupd than ABinderProcess_joinThreadPool();
    ABinderProcess_setupPolling(&daemon->binder_fd);
    g_idle_add(poll_binder_process, daemon);

    g_warning("starting main loop");
    g_main_loop_run(loop);

    return EXIT_SUCCESS;
}
