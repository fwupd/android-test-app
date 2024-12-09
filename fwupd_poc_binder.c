#include "config.h"
#include "gbinder_client.h"
#include "gbinder_reader.h"
#include "gbinder_types.h"
#include "gbinder_writer.h"
#include "glibconfig.h"
#include <glib.h>
#include <glib-unix.h>
#include <stdio.h>
#include <gbinder.h>
#include <string.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>
#if HAS_BINDER_NDK
#include <android/binder_status.h>
#include <android/binder_parcel.h>
#include <android/persistable_bundle.h>
#endif /* HAS_BINDER_NDK */

#include "gparcelable.h"

#define DEFAULT_DEVICE "/dev/binder"
#define RPC_PROTO "aidl3"


#define PROJECT_NAME "fwupd_poc"
#define PROJECT_IFACE "org.freedesktop.fwupd_poc"

#define SERVICE_IFACE "org.freedesktop.fwupdpoc.IPocFwupd"
#define LISTENER_IFACE "org.freedesktop.fwupdpoc.IPocFwupdListener"

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
    GET_BUNDLE,
    GET_BUNDLES,
    GET_BUNDLE_MATRIX,
    GET_STRINGS,
    GET_BOOLS,
    GET_MAYBE_STRINGS,
    ADD_LISTENER,
    REMOVE_LISTENER,
    TRIGGER_CHANGE,
    TRIGGER_DEVICE_ADDED,
};

enum listener_transactions {
    FWUPD_LISTENER_ON_CHANGED = GBINDER_FIRST_CALL_TRANSACTION,
    FWUPD_LISTENER_ON_DEVICE_ADDED,
    FWUPD_LISTENER_ON_DEEP_EXAMPLE,
    FWUPD_LISTENER_ON_PARAMS,
    FWUPD_LISTENER_ON_BUNDLES,
};

struct PocDaemon {
    GBinderServiceManager *sm;
    GBinderLocalObject *service_obj;
    GPtrArray* listener_array;
};

struct ClientCallbacks {
    void (*onChanged)(void);
    void (*onDeviceAdded)(void);
};

struct Device {
    int id;
    int ido;
    char* path;
};
G_STATIC_ASSERT(sizeof(struct Device) == 16);

struct OtherExample {
    int id;
    char* example;
};
G_STATIC_ASSERT(sizeof(struct OtherExample) == 16);

struct DeepExample {
    struct OtherExample *other;
    int id;
    char* example;
};
G_STATIC_ASSERT(sizeof(struct DeepExample) == 24);

#if HAS_BINDER_NDK

G_DEFINE_AUTOPTR_CLEANUP_FUNC(AStatus, AStatus_delete)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(AParcel, AParcel_delete)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(APersistableBundle, APersistableBundle_delete)


// TODO: Create a standard error macro for AParcel types
//   When Coul
//if ((status = APersistableBundle_writeToParcel(bundle, parcel)) != GBINDER_STATUS_OK) {
//    g_warning("Failed to write bundle %d", status);
//    return status;
//}
#define PARCEL_ASSERT_OK(status) (if ((nstatus = (status)) != STATUS_OK) {\
    g_warning("Failed to create Parcel for variant of type \"%s\" %d", g_variant_get_type_string(variant), nstatus);\
    return status;\
})

static binder_status_t poc_parcel_write_variant(AParcel *parcel, GVariant *variant);

// IIRC GVariant has the ability to register transform things
// fwupd devices are an array of dicts with strings, uint64s and arrays of strings
APersistableBundle *
poc_vardict_to_bundle(GVariant *vardict) {
    g_autoptr(APersistableBundle) bundle = APersistableBundle_new();
    GVariantIter iter;
    g_variant_iter_init(&iter, vardict);
    const gchar *key;
    GVariant *value;

    while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
        const GVariantType *const type = g_variant_get_type(value);
        const GVariantType *element_type = NULL;
        GVariantClass vclass = g_variant_classify(value);
        gconstpointer array_data = NULL;
        gsize fixed_array_size = 0;
        g_autoptr(APersistableBundle) child_bundle = NULL;
        // TODO: Could I (should I) use fallthrough with gint32 v = &g_variant_get_data(value) to marshall un/signed variables
        switch (vclass) {
        case G_VARIANT_CLASS_BOOLEAN:
            APersistableBundle_putBoolean(bundle, key, g_variant_get_boolean(value));
            break;
        case G_VARIANT_CLASS_BYTE:
            APersistableBundle_putInt(bundle, key, g_variant_get_byte(value));
            break;
        case G_VARIANT_CLASS_INT16:
        case G_VARIANT_CLASS_UINT16:
            APersistableBundle_putInt(bundle, key, *((guint16*)g_variant_get_data(value)));
            break;
        case G_VARIANT_CLASS_INT32:
        case G_VARIANT_CLASS_UINT32:
            APersistableBundle_putInt(bundle, key, *((guint32*)g_variant_get_data(value)));
            break;
        case G_VARIANT_CLASS_INT64:
        case G_VARIANT_CLASS_UINT64:
            APersistableBundle_putInt(bundle, key, *((guint64*)g_variant_get_data(value)));
            break;
        case G_VARIANT_CLASS_DOUBLE:
            APersistableBundle_putDouble(bundle, key, g_variant_get_double(value));
            break;
        case G_VARIANT_CLASS_STRING:
            APersistableBundle_putString(bundle, key, g_variant_get_string(value, NULL));
            break;
        // case G_VARIANT_CLASS_OBJECT_PATH: DBus specific (string)
        // case G_VARIANT_CLASS_SIGNATURE: GVariant specific? (string)
        // case G_VARIANT_CLASS_VARIANT: Not possible (Bundles support putParcelable PersistableBundles do not)
        // case G_VARIANT_CLASS_MAYBE: maybe useful
        case G_VARIANT_CLASS_ARRAY:
            element_type = g_variant_type_element(type);
            // TODO: This is effectively how g_variant_classify works, but is it okay to do outside the library?
            switch (g_variant_type_peek_string(element_type)[0]) {
            case G_VARIANT_CLASS_BOOLEAN:
                array_data = g_variant_get_fixed_array(value, &fixed_array_size, sizeof(guchar));
                APersistableBundle_putBooleanVector(bundle, key, array_data, fixed_array_size);
                break;
            case G_VARIANT_CLASS_INT32:
            case G_VARIANT_CLASS_UINT32:
                array_data = g_variant_get_fixed_array(value, &fixed_array_size, sizeof(guint32));
                APersistableBundle_putIntVector(bundle, key, array_data, fixed_array_size);
                break;
            case G_VARIANT_CLASS_INT64:
            case G_VARIANT_CLASS_UINT64:
                array_data = g_variant_get_fixed_array(value, &fixed_array_size, sizeof(guint64));
                APersistableBundle_putLongVector(bundle, key, array_data, fixed_array_size);
                break;
            case G_VARIANT_CLASS_DOUBLE:
                array_data = g_variant_get_fixed_array(value, &fixed_array_size, sizeof(gdouble));
                APersistableBundle_putDoubleVector(bundle, key, array_data, fixed_array_size);
                break;
            case G_VARIANT_CLASS_STRING:
                array_data = g_variant_get_strv(value, &fixed_array_size);
                APersistableBundle_putStringVector(bundle, key, array_data, fixed_array_size);
                break;
            case G_VARIANT_CLASS_DICT_ENTRY:
                /* Not an array, a vardict */
                /* putPersistableBundle deep copies the child bundle */
                child_bundle = poc_vardict_to_bundle(value);
                APersistableBundle_putPersistableBundle(bundle, key, child_bundle);
                break;
            default:
                g_warning("Couldn't add %s of type %s to PersistableBundle", key, g_variant_get_type_string(value));
            }
            //if (g_variant_type_equal(element_type, G_VARIANT_TYPE_BOOLEAN)) {
            //    gconstpointer bools = g_variant_get_fixed_array(value, &fixed_array_size, sizeof(guchar));
            //    APersistableBundle_putBooleanVector(bundle, key, bools, fixed_array_size);
            //}
            //switch ();
            //TODO: PersistableBundle can have {Bool Int Long Double String}
            break;
        // case G_VARIANT_CLASS_TUPLE: probably not useful
        // case G_VARIANT_CLASS_DICT_ENTRY:
        default:
            g_warning("Couldn't add %s of type %s to PersistableBundle", key, g_variant_get_type_string(value));
        }
        g_variant_unref (value);
    }

    return g_steal_pointer(&bundle);
}

static binder_status_t
poc_parcel_variant_array_element_writer(AParcel* parcel, const void* array_data, size_t index)
{
    GVariantIter *array_iter = (GVariantIter *)array_data;
    // TODO: Are we safe to ignore index
    GVariant *variant = g_variant_iter_next_value(array_iter);

    g_debug("poc_parcel_variant_array_element_writer %ld %s", index, g_variant_get_type_string(variant));

    return poc_parcel_write_variant(parcel, variant);
}

static const char*
poc_parcel_string_array_element_getter(const void *array_data, gsize index, gint32 *string_length)
{
    GVariant *strv_variant = (GVariant *)array_data;
    //GVariantIter *array_iter = (GVariantIter *)array_data;
    //GVariant * variant = g_variant_iter_next_value(array_iter);
    //gsize string_size = 0;
    //const gchar *string = g_variant_get_string(variant, &string_size);
    //*string_length = string_size;
    //return string;
    const gchar *string;
    g_variant_get_child(strv_variant, index, "s", &string);
    *string_length = strlen(string);
    return string;
}

static bool
poc_parcel_boolean_array_element_getter(const void *array_data, gsize index)
{
    GVariant *strv_variant = (GVariant *)array_data;
    gboolean value;
    g_variant_get_child(strv_variant, index, "b", &value);
    return value;
}


static binder_status_t
poc_parcel_write_variant(AParcel *parcel, GVariant *variant) {
    GVariantIter iter;
    GVariantClass vclass = g_variant_classify(variant);
    const GVariantType *const type = g_variant_get_type(variant);
    const GVariantType *element_type = NULL;
    g_autoptr(APersistableBundle) bundle = NULL;
    binder_status_t status = STATUS_OK;
    gconstpointer array_data = NULL;
    gsize fixed_array_size = 0;

    g_debug("poc_parcel_write_variant %s", g_variant_get_type_string(variant));
    // TODO: What values are we interested in?
    //   Parcel can contain:
    //    * [-] StrongBinder?
    //    * [-] FD
    //    * [-] StatusHeader
    //    * [x] String
    //    * [ ] StringArray
    //    * [x] ParcelableArray (implemented)
    //    * [ ] i32, u32, i64, u64, float, double, bool, char, byte and arrays of the aforementioned
    //        confusingly whilst the api provides a way to write u32, AIDL doesn't allow you to make a variable unsigned
    // TODO: this could also be a dictionary, etc.
    g_debug(" - - hello world %c %s %s", vclass, vclass == G_VARIANT_CLASS_ARRAY ? "is array" : "not array", g_variant_type_peek_string(type));
    switch (vclass) {
    case G_VARIANT_CLASS_BOOLEAN:
        AParcel_writeBool(parcel, g_variant_get_boolean(variant));
        break;
    case G_VARIANT_CLASS_BYTE:
        AParcel_writeByte(parcel, g_variant_get_byte(variant));
        break;
    case G_VARIANT_CLASS_INT32:
        AParcel_writeInt32(parcel, g_variant_get_int32(variant));
        break;
    case G_VARIANT_CLASS_UINT32:
        AParcel_writeUint32(parcel, g_variant_get_uint32(variant));
        break;
    case G_VARIANT_CLASS_INT64:
        AParcel_writeInt64(parcel, g_variant_get_int64(variant));
        break;
    case G_VARIANT_CLASS_UINT64:
        AParcel_writeUint64(parcel, g_variant_get_int64(variant));
        break;
    case G_VARIANT_CLASS_DOUBLE:
        AParcel_writeDouble(parcel, g_variant_get_double(variant));
        break;
    case G_VARIANT_CLASS_STRING:
        //string = g_variant_get_string(variant, &fixed_array_size);
        // TODO: do i need to write string16?
        // Can I do this or is fixed_array_size not ready
        AParcel_writeString(parcel, g_variant_get_string(variant, &fixed_array_size), fixed_array_size);
        break;

    /**
     * Array covers the following data types we can handle:
     *  + vardict `a{sv}`
     *  + typed arrays `ax` where x is a basic type
     *  + Array of parcelables `ax` where x is one of vardict, array, ???
     **/
    case G_VARIANT_CLASS_ARRAY:
        element_type = g_variant_type_element(type);
        switch (g_variant_type_peek_string(element_type)[0]) {
        case G_VARIANT_CLASS_DICT_ENTRY:
        /* Write a PersistableBundle */
            /* Convert */
            bundle = poc_vardict_to_bundle(variant);
            /* Write nullable flag */
            AParcel_writeInt32(parcel, !!bundle);
            /* Write persistable bundle */
            if ((status = APersistableBundle_writeToParcel(bundle, parcel)) != GBINDER_STATUS_OK) {
                g_warning("Failed to write bundle %d", status);
                return status;
            }
            break;
        // TODO: AParcel_boolArrayGetter? To get the size right?
        case G_VARIANT_CLASS_BOOLEAN:
        //    array_data = g_variant_get_fixed_array(variant, &fixed_array_size, sizeof(guchar));
            if ((status = AParcel_writeBoolArray(parcel, variant, g_variant_n_children(variant),
                poc_parcel_boolean_array_element_getter)) != STATUS_OK) {
                g_warning("Failed to write bool array %d", status);
                return status;
            }
            break;
        // TODO: AParcelCharArray takes char16_t
        case G_VARIANT_CLASS_BYTE:
            array_data = g_variant_get_fixed_array(variant, &fixed_array_size, sizeof(guint8));
            AParcel_writeByteArray(parcel, array_data, fixed_array_size);
            break;
        case G_VARIANT_CLASS_INT32:
            array_data = g_variant_get_fixed_array(variant, &fixed_array_size, sizeof(gint32));
            AParcel_writeInt32Array(parcel, array_data, fixed_array_size);
            break;
        case G_VARIANT_CLASS_UINT32:
            array_data = g_variant_get_fixed_array(variant, &fixed_array_size, sizeof(guint32));
            AParcel_writeUint32Array(parcel, array_data, fixed_array_size);
            break;
        case G_VARIANT_CLASS_INT64:
            array_data = g_variant_get_fixed_array(variant, &fixed_array_size, sizeof(gint64));
            AParcel_writeInt64Array(parcel, array_data, fixed_array_size);
            break;
        case G_VARIANT_CLASS_UINT64:
            array_data = g_variant_get_fixed_array(variant, &fixed_array_size, sizeof(guint64));
            AParcel_writeUint64Array(parcel, array_data, fixed_array_size);
            break;
        case G_VARIANT_CLASS_DOUBLE:
            /* parcels support float and double, variants only double */
            array_data = g_variant_get_fixed_array(variant, &fixed_array_size, sizeof(gdouble));
            AParcel_writeDoubleArray(parcel, array_data, fixed_array_size);
            break;
        case G_VARIANT_CLASS_STRING:
        // TODO: AParcel_stringArrayElementGetter
            AParcel_writeStringArray(parcel, variant, g_variant_n_children(variant), poc_parcel_string_array_element_getter);
            break;
        default:
            // TODO: Do I need to specify types that can be attempted this way?
            g_variant_iter_init(&iter, variant);
            AParcel_writeParcelableArray(parcel, &iter,
                g_variant_iter_n_children(&iter), poc_parcel_variant_array_element_writer);
            break;
        }
        break;
    case G_VARIANT_CLASS_TUPLE:
        // TODO: These would probably be extremely useful for passing arguments
        // iter tuple children calling parcel_write_variant, I think? Parcels are tuples?
        g_warning("tuples not implemented yet");
        break;
    default:
        g_warning("Cannot encode type \"%s\" as Parcel", g_variant_get_type_string(variant));
    }
    /*
    if (g_variant_type_is_array(g_variant_get_type(variant))) {
        element_type = g_variant_type_element(g_variant_get_type(variant));
        g_info("  poc_parcel_write_variant it's an array element type: %s, %s",
            g_variant_type_peek_string(element_type), g_variant_type_is_array(element_type) ? "is array" : "not array");

        g_variant_iter_init(&iter, variant);
        //g_variant_get_
        // TODO: Do I iterate an "av" into a fixed array or do I use get_fixed_array
        // THERE IS NO NULLABLE HERE!
        //AParcel_writeInt32(parcel, 1);
        AParcel_writeParcelableArray(parcel, &iter,
            g_variant_iter_n_children(&iter), poc_parcel_variant_array_element_writer);
    }
    */

    return STATUS_OK;
}

binder_status_t
poc_write_gvariant_element(AParcel* parcel, const void* array_data, size_t index)
{
    GPtrArray *variant_array = (GPtrArray*)array_data;
    g_autoptr(GVariant) variant = g_ptr_array_index(variant_array, index);

    g_autoptr(APersistableBundle) bundle = poc_vardict_to_bundle(variant);

    binder_status_t status = STATUS_OK;

    // NULL??
    AParcel_writeInt32(parcel, 1);
    if ((status = APersistableBundle_writeToParcel(bundle, parcel)) != GBINDER_STATUS_OK) {
        g_warning("Failed to write bundle %d", status);
        // g_set_error()???
        return status;
    }

    return status;
}

#endif /* HAS_BINDER_NDK */

GVariant *
poc_create_vardict(GVariant* child) {
    GVariantDict vardict;
    g_variant_dict_init(&vardict, NULL);
    g_variant_dict_insert(&vardict, "string", "s", "hello world");
    g_variant_dict_insert(&vardict, "u8", "y", 101);
    g_variant_dict_insert(&vardict, "u16", "n", 101);
    g_variant_dict_insert(&vardict, "u32", "u", 101);
    g_variant_dict_insert(&vardict, "f64", "d", 101.0);
    g_variant_dict_insert(&vardict, "max_u8", "y", G_MAXUINT8);
    g_variant_dict_insert(&vardict, "max_u16", "n", G_MAXUINT16);
    g_variant_dict_insert(&vardict, "max_i16", "q", G_MAXINT16);
    g_variant_dict_insert(&vardict, "max_u32", "u", G_MAXUINT32);
    g_variant_dict_insert(&vardict, "max_i32", "i", G_MAXINT32);

    // string array
    const char* strarray[] = {"hehe", "hoho", "haha"};
    GVariant * strvec = g_variant_new_strv(strarray, G_N_ELEMENTS(strarray));
    g_variant_dict_insert_value(&vardict, "strvec", strvec);

    // bool array
    const guchar boolarray[] = {
        TRUE, FALSE, FALSE, TRUE,
        TRUE, FALSE, FALSE, TRUE,
        TRUE,
        };
    GVariant * boolvec = g_variant_new_fixed_array(G_VARIANT_TYPE_BOOLEAN, boolarray, G_N_ELEMENTS(boolarray), sizeof(guchar));
    g_variant_dict_insert_value(&vardict, "boolvec", boolvec);

    // int array
    const gint32 intarray[] = {
        101, 1, 2, 3, 4, 101,
    };
    GVariant * intvec = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32, intarray, G_N_ELEMENTS(intarray), sizeof(gint32));
    g_variant_dict_insert_value(&vardict, "intvec", intvec);

    // long array
    const gint64 longarray[] = {
        101, 1, 2, 3, 4, 101,
    };
    GVariant * longvec = g_variant_new_fixed_array(G_VARIANT_TYPE_INT64, longarray, G_N_ELEMENTS(longarray), sizeof(gint64));
    g_variant_dict_insert_value(&vardict, "longvec", longvec);

    // double array
    const gdouble doublearray[] = {
        1.01, 0.0, 0.1, 0.2, 0.3, 1.01,
    };
    GVariant * doublevec = g_variant_new_fixed_array(G_VARIANT_TYPE_DOUBLE, doublearray, G_N_ELEMENTS(doublearray), sizeof(gdouble));
    g_variant_dict_insert_value(&vardict, "doublevec", doublevec);

    if (child != NULL)
        g_variant_dict_insert_value(&vardict, "vardict", child);
    return g_variant_dict_end(&vardict);
}

GVariant *
poc_create_vardict_small(void) {
    GVariantDict vardict;
    g_variant_dict_init(&vardict, NULL);
    g_variant_dict_insert(&vardict, "string", "s", "hello world");
    g_variant_dict_insert(&vardict, "u8", "y", 101);
    return g_variant_dict_end(&vardict);
}

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
    GBinderWriter bundle_writer;
    g_autoptr(AStatus) status_header = AStatus_newOk();
    binder_status_t nstatus = STATUS_OK;
    gbinder_remote_request_init_reader(req, &reader);
    g_autofree gchar *str2 = g_strdup_printf("here is the message for the client");
    const gchar *iface = gbinder_remote_request_interface(req);

    g_message("received %s %d", iface, code);
    if (g_str_equal(iface, SERVICE_IFACE)) {
        switch (code) {
            case GET_STRING:
                reply = gbinder_local_object_new_reply(obj);

                gbinder_local_reply_init_writer(reply, &bundle_writer);
                //gbinder_local_reply_append_int32(reply, GBINDER_STATUS_OK);
                //gbinder_local_reply_append_string16(reply, str2);

                //*status = GBINDER_STATUS_OK;

#if HAS_BINDER_NDK
            {
                gint size;

                g_autoptr(AParcel) parcel = AParcel_create();

                //g_autoptr(GVariant) string_variant = g_variant_new_string("const gchar *string");
                g_autoptr(GVariant) string_variant = g_variant_new("mi", NULL);
                // reply status ok
                AParcel_writeStatusHeader(parcel, status_header);
                GError *error = NULL;
                gp_parcel_write_variant(parcel, string_variant, &error);
                if (error) {
                    g_error("Error %s", error->message);
                    return NULL;
                }

                size = AParcel_getDataSize(parcel);
                g_autofree uint8_t* buffer = calloc(1, size);
                //g_return_if_val_fail(((nstatus = AParcel_marshal(parcel, buffer, 0, size)) == STATUS_OK), nstatus);
                nstatus = AParcel_marshal(parcel, buffer, 0, size);
                if (nstatus != GBINDER_STATUS_OK) {
                    g_warning("Failed to marshal parcel %d", nstatus);
                    return NULL;
                }

                gbinder_writer_append_bytes(&bundle_writer, buffer, size);
            }
#endif /* HAS_BINDER_NDK */
                g_debug("get_string \"%s\"", str2);
                return reply;
                break;
            case SET_STRING:
            {
                gchar *str = gbinder_reader_read_string16(&reader);
                g_debug("set_string \"%s\"", str);
            }
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
            {
                g_warning("set_dict unimplemented");
            }
                break;
            case GET_INT:
            {
                gint32 int_value = 99;
                reply = gbinder_local_object_new_reply(obj);

                gbinder_local_reply_append_int32(reply, GBINDER_STATUS_OK);
                gbinder_local_reply_append_int32(reply, int_value);

                g_debug("get_int \"%d\"", int_value);
                return reply;
            }
                break;
            case SET_INT:
            {
                gint32 int_value2 = 0;
                gbinder_reader_read_int32(&reader, &int_value2);
                g_debug("set_int \"%d\"", int_value2);
            }
                break;
            case GET_INT_ARRAY:
            {
                reply = gbinder_local_object_new_reply(obj);
                gbinder_local_reply_init_writer(reply, &bundle_writer);

#if HAS_BINDER_NDK
                gint size;
                gint32 int_array[] = {
                    101, 3, 2, 1, 0, 101,
                };

                g_autoptr(GVariant) int_array_variant = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32, int_array, G_N_ELEMENTS(int_array), sizeof(gint32));

                g_autoptr(AParcel) parcel = AParcel_create();

                // reply status ok
                AParcel_writeStatusHeader(parcel, status_header);
                gp_parcel_write_variant(parcel, int_array_variant, NULL);

                size = AParcel_getDataSize(parcel);
                g_autofree uint8_t* buffer = calloc(1, size);
                if ((nstatus = AParcel_marshal(parcel, buffer, 0, size)) != GBINDER_STATUS_OK) {
                    g_warning("Failed to marshal parcel %d", nstatus);
                }

                gbinder_writer_append_bytes(&bundle_writer, buffer, size);
#endif /* HAS_BINDER_NDK */
                g_debug("get_int_array");
                return reply;
                break;
            }
            case GET_BUNDLE:
            {
                reply = gbinder_local_object_new_reply(obj);
                gbinder_local_reply_init_writer(reply, &bundle_writer);

#if HAS_BINDER_NDK
                gint size;

                GVariant * vardicta = poc_create_vardict(NULL);
                GVariant * vardict = poc_create_vardict(vardicta);

                APersistableBundle * bundle = gp_vardict_to_persistable_bundle(vardict, NULL);

                AParcel * parcel = AParcel_create();

                // reply status ok
                AParcel_writeStatusHeader(parcel, status_header);

                // not-null flag
                AParcel_writeInt32(parcel, !!bundle);

                g_info("persistable bundle size is %d", APersistableBundle_size(bundle));

                if ((nstatus = APersistableBundle_writeToParcel(bundle, parcel)) != GBINDER_STATUS_OK) {
                    g_warning("Failed to write bundle %d", nstatus);
                }

                size = AParcel_getDataSize(parcel);
                g_autofree uint8_t* buffer = calloc(1, size);
                if ((nstatus = AParcel_marshal(parcel, buffer, 0, size)) != GBINDER_STATUS_OK) {
                    g_warning("Failed to marshal parcel %d", nstatus);
                }

                gbinder_writer_append_bytes(&bundle_writer, buffer, size);
#endif /* HAS_BINDER_NDK */
                g_debug("get_bundle");
                return reply;
                break;
            }
            case GET_BUNDLES:
            {
                reply = gbinder_local_object_new_reply(obj);
                gbinder_local_reply_init_writer(reply, &bundle_writer);

#if HAS_BINDER_NDK

                gint size;

                g_autoptr(GVariant) vardicta = poc_create_vardict(NULL);
                g_autoptr(GVariant) vardict = poc_create_vardict(vardicta);

                AParcel * packet_parcel = AParcel_create();

                // reply status ok
                AParcel_writeStatusHeader(packet_parcel, status_header);

                GVariantBuilder builder;
                g_variant_builder_init(&builder, G_VARIANT_TYPE("aa{sv}"));
                g_variant_builder_add_value(&builder, vardict);
                g_variant_builder_add_value(&builder, vardicta);
                GVariant *variant_array_variant = g_variant_builder_end(&builder);

                gp_parcel_write_variant(packet_parcel, variant_array_variant, NULL);

                size = AParcel_getDataSize(packet_parcel);
                g_autofree uint8_t* buffer = calloc(1, size);
                if ((nstatus = AParcel_marshal(packet_parcel, buffer, 0, size)) != GBINDER_STATUS_OK) {
                    g_warning("Failed to marshal parcel %d", nstatus);
                }

                gbinder_writer_append_bytes(&bundle_writer, buffer, size);
#endif /* HAS_BINDER_NDK */
                g_debug("get_bundles");
                return reply;
            }
                break;

            case GET_BUNDLE_MATRIX:
            {
                reply = gbinder_local_object_new_reply(obj);
                gbinder_local_reply_init_writer(reply, &bundle_writer);

#if HAS_BINDER_NDK
                gint size;

                g_autoptr(GVariant) vardict = poc_create_vardict_small();

                g_autoptr(AParcel) packet_parcel = AParcel_create();

                // reply status ok
                AParcel_writeStatusHeader(packet_parcel, status_header);

                GVariantBuilder builder;
                g_variant_builder_init(&builder, G_VARIANT_TYPE("aa{sv}"));
                g_variant_builder_add_value(&builder, vardict);
                g_variant_builder_add_value(&builder, vardict);
                g_autoptr(GVariant) variant_array_variant = g_variant_builder_end(&builder);
                g_assert_nonnull(variant_array_variant);
                g_variant_builder_clear(&builder);

                g_variant_builder_init(&builder, G_VARIANT_TYPE("aaa{sv}"));
                g_variant_builder_add_value(&builder, g_variant_ref(variant_array_variant));
                g_variant_builder_add_value(&builder, g_variant_ref(variant_array_variant));
                g_autoptr(GVariant) variant_array_array_variant = g_variant_builder_end(&builder);

                gp_parcel_write_variant(packet_parcel, variant_array_array_variant, NULL);

                size = AParcel_getDataSize(packet_parcel);
                g_autofree uint8_t* buffer = calloc(1, size);
                if ((nstatus = AParcel_marshal(packet_parcel, buffer, 0, size)) != GBINDER_STATUS_OK) {
                    g_warning("Failed to marshal parcel %d", nstatus);
                }

                gbinder_writer_append_bytes(&bundle_writer, buffer, size);
#endif /* HAS_BINDER_NDK */
                g_debug("get_bundle_matrix");
                return reply;

            }
                break;
            case GET_STRINGS:
            {
                reply = gbinder_local_object_new_reply(obj);
                gbinder_local_reply_init_writer(reply, &bundle_writer);

#if HAS_BINDER_NDK
                gint size;

                AParcel * packet_parcel = AParcel_create();

                // reply status ok
                AParcel_writeStatusHeader(packet_parcel, status_header);

                GVariantBuilder builder;
                g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
                g_variant_builder_add(&builder, "s" ,"tom");
                g_variant_builder_add(&builder, "s" ,"dick");
                g_variant_builder_add(&builder, "s" ,"larry");
                g_autoptr(GVariant) variant_array_variant = g_variant_builder_end(&builder);

                g_assert_nonnull(variant_array_variant);
                gp_parcel_write_variant(packet_parcel, variant_array_variant, NULL);

                size = AParcel_getDataSize(packet_parcel);
                g_autofree uint8_t* buffer = calloc(1, size);
                if ((nstatus = AParcel_marshal(packet_parcel, buffer, 0, size)) != GBINDER_STATUS_OK) {
                    g_warning("Failed to marshal parcel %d", nstatus);
                }

                gbinder_writer_append_bytes(&bundle_writer, buffer, size);
#endif /* HAS_BINDER_NDK */
                g_debug("get_bundle_matrix");
                return reply;

            }
                break;
            case GET_BOOLS:
            {
                reply = gbinder_local_object_new_reply(obj);
                gbinder_local_reply_init_writer(reply, &bundle_writer);

#if HAS_BINDER_NDK
                gint size;

                AParcel * packet_parcel = AParcel_create();

                // reply status ok
                AParcel_writeStatusHeader(packet_parcel, status_header);

                g_auto(GVariantBuilder) builder;
                g_variant_builder_init(&builder, G_VARIANT_TYPE("ab"));
                g_variant_builder_add(&builder, "b", false);
                g_variant_builder_add(&builder, "b", true);
                g_variant_builder_add(&builder, "b", true);
                g_variant_builder_add(&builder, "b", false);
                g_variant_builder_add(&builder, "b", false);
                g_variant_builder_add(&builder, "b", true);
                g_variant_builder_add(&builder, "b", true);
                g_variant_builder_add(&builder, "b", false);
                g_variant_builder_add(&builder, "b", false);
                
                g_autoptr(GVariant) variant_array_variant = g_variant_builder_end(&builder);
                //g_info("variant_array_variant_array %p %ld", variant_array_variant_array, G_N_ELEMENTS(variant_array_variant_array));
                //GVariant* variant_array_variant = g_variant_new_array(NULL,
                //    variant_array_variant_array, G_N_ELEMENTS(variant_array_variant_array));
                g_assert_nonnull(variant_array_variant);
                gp_parcel_write_variant(packet_parcel, variant_array_variant, NULL);

                size = AParcel_getDataSize(packet_parcel);
                g_autofree uint8_t* buffer = calloc(1, size);
                if ((nstatus = AParcel_marshal(packet_parcel, buffer, 0, size)) != GBINDER_STATUS_OK) {
                    g_warning("Failed to marshal parcel %d", nstatus);
                }

                gbinder_writer_append_bytes(&bundle_writer, buffer, size);
#endif /* HAS_BINDER_NDK */
                g_debug("get_bundle_matrix");
                return reply;

            }
                break;
            case GET_MAYBE_STRINGS:
            {
                reply = gbinder_local_object_new_reply(obj);
                gbinder_local_reply_init_writer(reply, &bundle_writer);

#if HAS_BINDER_NDK
                gint size;

                AParcel * packet_parcel = AParcel_create();

                // reply status ok
                AParcel_writeStatusHeader(packet_parcel, status_header);

                GVariantBuilder builder;
                g_variant_builder_init(&builder, G_VARIANT_TYPE("ams"));
                g_variant_builder_add(&builder, "ms" ,"tom");
                g_variant_builder_add(&builder, "ms" ,"dick");
                g_variant_builder_add(&builder, "ms" ,"larry");
                g_variant_builder_add(&builder, "ms" ,NULL);
                g_autoptr(GVariant) variant_array_variant = g_variant_builder_end(&builder);

                g_assert_nonnull(variant_array_variant);
                gp_parcel_write_variant(packet_parcel, variant_array_variant, NULL);

                size = AParcel_getDataSize(packet_parcel);
                g_autofree uint8_t* buffer = calloc(1, size);
                if ((nstatus = AParcel_marshal(packet_parcel, buffer, 0, size)) != GBINDER_STATUS_OK) {
                    g_warning("Failed to marshal parcel %d", nstatus);
                }

                gbinder_writer_append_bytes(&bundle_writer, buffer, size);
#endif /* HAS_BINDER_NDK */
                g_debug("get_maybe_strings");
                return reply;

            }
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
                    GBinderClient *listener_client = gbinder_client_new(listener_obj, LISTENER_IFACE);
                    GBinderLocalRequest* listener_req  = gbinder_client_new_request(listener_client);
                    gint listener_req_status;

                    gbinder_client_transact_sync_reply(listener_client, FWUPD_LISTENER_ON_CHANGED, listener_req, &listener_req_status);

                    gsize size;
                    // TODO: If string parameter is null this isn't called?
                    /* ms allows string to be passed null, the client must define the parameter as String? */
                    g_autoptr(GVariant) params = g_variant_new("(idmsb)", 1, 1.1, NULL, true);
                    g_autoptr(AParcel) params_parcel = AParcel_create();
                    nstatus = gp_parcel_write_variant(params_parcel, params, NULL);

                    listener_req  = gbinder_client_new_request(listener_client);
                    GBinderWriter params_writer;
                    gbinder_local_request_init_writer(listener_req, &params_writer);

                    size = AParcel_getDataSize(params_parcel);
                    g_autofree uint8_t *buffer = calloc(1, size);
                    if ((nstatus = AParcel_marshal(params_parcel, buffer, 0, size)) != GBINDER_STATUS_OK) {
                        g_warning("Failed to marshal parcel %d", nstatus);
                    }

                    gbinder_writer_append_bytes(&params_writer, buffer, size);

                    // TODO: Should I be using gbinder_client_transact{,_oneway,_reply}?
                    // TODO: Marshal params_parcel and write it to params_writer
                    gbinder_client_transact_sync_reply(listener_client, FWUPD_LISTENER_ON_PARAMS, listener_req, &listener_req_status);


                    g_autoptr(GVariant) vardict_small = poc_create_vardict_small();

                    GVariantBuilder builder;
                    g_variant_builder_init(&builder, G_VARIANT_TYPE("aa{sv}"));
                    g_variant_builder_add_value(&builder, vardict_small);
                    g_variant_builder_add_value(&builder, vardict_small);
                    g_autoptr(GVariant) variant_array_variant = g_variant_builder_end(&builder);
                    g_assert_nonnull(variant_array_variant);
                    g_variant_builder_clear(&builder);
                    g_debug(" - - - this here array of things %s", g_variant_get_type_string(variant_array_variant));

                    g_variant_builder_init(&builder, G_VARIANT_TYPE("(imaa{sv}ms)"));
                    g_variant_builder_add(&builder, "i", 1);
                    g_debug(" - - - open");
                    //g_variant_builder_add(&builder, "maa{sv}", NULL);
                    g_variant_builder_open(&builder, G_VARIANT_TYPE("maa{sv}"));
                    g_variant_builder_open(&builder, G_VARIANT_TYPE("aa{sv}"));
                    g_variant_builder_add_value(&builder, g_variant_ref(vardict_small));
                    g_variant_builder_close(&builder);

                    //g_variant_builder_add_value(&builder, g_variant_ref(vardict_small));
                    g_debug(" - - - close");
                    g_variant_builder_close(&builder);
                    //g_variant_builder_add_value(&builder, g_variant_ref(variant_array_variant));
                    g_variant_builder_add(&builder, "ms", "None");
                    //g_autoptr(GVariant) bundles_params = g_variant_new("(imaa{sv}ms)", 0, variant_array_variant, "buub");
                    g_debug(" - - - end");
                    g_autoptr(GVariant) bundles_params = g_variant_builder_end(&builder);
                    g_assert_nonnull(bundles_params);

                    g_debug(" - - - this here tuple of array of things %s", g_variant_get_type_string(bundles_params));

                    g_autoptr(AParcel) bundles_params_parcel = AParcel_create();
                    nstatus = gp_parcel_write_variant(bundles_params_parcel, bundles_params, NULL);

                    listener_req  = gbinder_client_new_request(listener_client);
                    //GBinderWriter params_writer;
                    gbinder_local_request_init_writer(listener_req, &params_writer);

                    size = AParcel_getDataSize(bundles_params_parcel);
                    g_autofree uint8_t *bundles_params_buffer = calloc(1, size);
                    if ((nstatus = AParcel_marshal(bundles_params_parcel, bundles_params_buffer, 0, size)) != GBINDER_STATUS_OK) {
                        g_warning("Failed to marshal parcel %d", nstatus);
                    }

                    gbinder_writer_append_bytes(&params_writer, bundles_params_buffer, size);

                    // TODO: Should I be using gbinder_client_transact{,_oneway,_reply}?
                    // TODO: Marshal params_parcel and write it to params_writer
                    gbinder_client_transact_sync_reply(listener_client, FWUPD_LISTENER_ON_BUNDLES, listener_req, &listener_req_status);
                    if (listener_req_status != STATUS_OK)
                        g_debug(" - - transaction failed for onBundles %d", listener_req_status);
                }

                g_debug("trigger_change");
                break;
            case TRIGGER_DEVICE_ADDED:
                for (guint i = 0; i < daemon->listener_array->len; i++ )
                {
                    GBinderRemoteObject *listener_obj = g_ptr_array_index(daemon->listener_array, i);
                    GBinderClient *listener_client = gbinder_client_new(listener_obj, LISTENER_IFACE);

                    GBinderLocalRequest* listener_req = NULL;
                    GBinderWriter device_writer;
                    gint listener_req_status;

#if HAS_BINDER_NDK
                    g_debug("send device 0 (ndk)");
                    binder_status_t nstatus = -1;
                    gint32 start, end, size;

                    AParcel* parcel = AParcel_create();

                    // kNonNullParcelableFlag in cpp
                    // NON_NULL_PARCELABLE_FLAG in rust
                    //AParcel_writeInt32(parcel, 1);
                    if ((nstatus = AParcel_writeInt32(parcel, 1)) != GBINDER_STATUS_OK) {
                        g_warning("Failed to write non-null flag %d", nstatus);
                    }

                    // Sized write
                    start = AParcel_getDataPosition(parcel);
                    g_debug("Parcel start %d", start);
                    // Allocate size value
                    if ((nstatus = AParcel_writeInt32(parcel, 0)) != GBINDER_STATUS_OK) {
                        g_warning("Failed to allocate parcel size %d", nstatus);
                    }

                    // Contents {
                    //  id
                    if ((nstatus = AParcel_writeInt32(parcel, 68)) != GBINDER_STATUS_OK) {
                        g_warning("Failed to write field \"id\" %d", nstatus);
                    }
                    //  ido
                    if ((nstatus = AParcel_writeInt32(parcel, 69)) != GBINDER_STATUS_OK) {
                        g_warning("Failed to write field \"ido\" %d", nstatus);
                    }
                    //  path
                    const char * str = "hello world";
                    if ((nstatus = AParcel_writeString(parcel, str, strlen(str))) != GBINDER_STATUS_OK) {
                        g_warning("Failed to write field \"path\" %d", nstatus);
                    }
                    // }

                    // End position
                    end = AParcel_getDataPosition(parcel);
                    g_debug("Parcel end %d", end);

                    // Write size to start
                    nstatus = AParcel_setDataPosition(parcel, start);
                    g_assert(end >= start);
                    if ((nstatus = AParcel_writeInt32(parcel, (end - start))) != GBINDER_STATUS_OK) {
                        g_warning("Failed to update parcel size %d", nstatus);
                    }
                    // Return cursor to end
                    nstatus = AParcel_setDataPosition(parcel, end);

                    // Marshal to buffer
                    size = AParcel_getDataSize(parcel);
                    g_autofree uint8_t* buffer = calloc(1, size);
                    AParcel_marshal(parcel, buffer, 0, size);

                    listener_req = gbinder_client_new_request(listener_client);
                    gbinder_local_request_init_writer(listener_req, &device_writer);
                    //gbinder_writer_append_parcelable(&device_writer, buffer, size);
                    gbinder_writer_append_bytes(&device_writer, buffer, size);
                    gbinder_client_transact_sync_reply(listener_client, FWUPD_LISTENER_ON_DEVICE_ADDED, listener_req, &listener_req_status);

                    free(buffer);
                    free(parcel);
#endif /* HAS_BINDER_NDK */

                    g_debug("send device 1");
                    // attempt 1 (works apart from strings)
                    listener_req = gbinder_client_new_request(listener_client);
                    // parcelable Device
                    gbinder_local_request_init_writer(listener_req, &device_writer);
                    /*
                    // sized_write
                    // get_data_position = 0 ( gbinder_writer_bytes_written?)
                    guint start = 0;
                    // header id for parcellable? (it contains the size, size is written here after it's decided)
                    gbinder_writer_append_int32(&device_writer, 8);
                    //  parcellable body {
                    //   id: String
                    gbinder_writer_append_string16(&device_writer, "hello from sunny libgbinder");
                    //  }
                    // get_data_position
                    guint end = 4 + 4; // sizeof(i32) + parcellable( sizeof(char*) )
                    */
                    
                    struct Device *device = gbinder_writer_new0(&device_writer, struct Device);
                    device->id = 12;
                    device->ido = 120;
                    device->path = gbinder_writer_strdup(&device_writer, "this is a test");
                    //device->path = "hello from sunny libgbinder";
                    g_info("appending buffer of size %lu", sizeof(*device));
                    gbinder_writer_append_parcelable(&device_writer, device, sizeof(*device));
                    //gbinder_writer_append_buffer_object(&device_writer, device, sizeof(*device));

                    gbinder_client_transact_sync_reply(listener_client, FWUPD_LISTENER_ON_DEVICE_ADDED, listener_req, &listener_req_status);

                    // attempt 2 (works with strings)

                    g_debug("send device 2");
                    listener_req  = gbinder_client_new_request(listener_client);
                    gbinder_local_request_init_writer(listener_req, &device_writer);
                    // not null
                    gbinder_writer_append_int32(&device_writer, 1);
                    // parcel size
                    gbinder_writer_append_int32(&device_writer, 16 + sizeof(gint32));
                    // id
                    gbinder_writer_append_int32(&device_writer, 13);
                    // ido
                    gbinder_writer_append_int32(&device_writer, 130);
                    // path
                    gbinder_writer_append_string16(&device_writer, "append each");

                    gbinder_client_transact_sync_reply(listener_client, FWUPD_LISTENER_ON_DEVICE_ADDED, listener_req, &listener_req_status);


                    g_debug("send deep example 1");
                    // DeepExample (doesn't work)

                    listener_req  = gbinder_client_new_request(listener_client);
                    gbinder_local_request_init_writer(listener_req, &device_writer);

                    struct DeepExample *de = gbinder_writer_new0(&device_writer, struct DeepExample);
                    //struct OtherExample *oe = gbinder_writer_new0(&device_writer, struct OtherExample);
                    //oe->id = 43;
                    de->other = NULL;//oe;
                    de->id = 43;
                    gbinder_writer_append_parcelable(&device_writer, de, sizeof(*de));

                    gbinder_client_transact_sync_reply(listener_client, FWUPD_LISTENER_ON_DEEP_EXAMPLE, listener_req, &listener_req_status);


                    g_debug("send deep example 2");
                    // DeepExample 2 (doesn't work)

                    listener_req  = gbinder_client_new_request(listener_client);
                    gbinder_local_request_init_writer(listener_req, &device_writer);
                    // not null
                    gbinder_writer_append_int32(&device_writer, 1);
                    // parcel size
                    gbinder_writer_append_int32(&device_writer, sizeof(struct DeepExample) + sizeof(struct OtherExample) + sizeof(gint32));
                    // not null (other)
                    gbinder_writer_append_int32(&device_writer, 1);
                    // other->id
                    gbinder_writer_append_int32(&device_writer, 3);
                    // other->example
                    gbinder_writer_append_string16(&device_writer, "append each");
                    // id
                    gbinder_writer_append_int32(&device_writer, 4);
                    // example
                    gbinder_writer_append_string16(&device_writer, "append each 2");

                    gbinder_client_transact_sync_reply(listener_client, FWUPD_LISTENER_ON_DEEP_EXAMPLE, listener_req, &listener_req_status);

                }
                g_debug("trigger_device_added");
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

    if (!gbinder_servicemanager_wait(sm, 1)) {
        g_warning("failed to wait for service manager");
        return NULL;
    }

    daemon = calloc(1, sizeof(*daemon));
    daemon->listener_array = g_ptr_array_new();
    daemon->sm = sm;

    // TODO: Many services list "null" as their interface
    // Service is listed as android.hidl.base@1.0::IBase if iface is NULL, and the string if it is not
    GBinderLocalObject *poc_service_obj = gbinder_servicemanager_new_local_object(sm, SERVICE_IFACE, handle_calls_cb, daemon);

    daemon->service_obj = poc_service_obj;

    gbinder_servicemanager_add_presence_handler(sm, handle_presence_cb, daemon);

    gbinder_servicemanager_add_service(sm, PROJECT_NAME, poc_service_obj, handle_add_service_cb, daemon);

    return daemon;
}


enum {
	NETLINK_GROUP_NONE = 0,
  NETLINK_GROUP_KERNEL,
  NETLINK_GROUP_UDEV,
};

void
start_netlink(void)
{
    struct sockaddr_nl nls = {
        .nl_family = AF_NETLINK,
        .nl_pid = getpid(),
        .nl_groups = NETLINK_GROUP_UDEV,
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
