PoC libgbinder based daemon and an idiomatic Android client.

This is to test and validate libgbinders ability to expose an Android service that can be consumed by an Android client using aidl bindings.

## Service/daemon

### Build

Setup the NDK path in `android_aarch64_cross_file.ini` and run
```bash
meson setup --cross-file ../fwupd/contrib/ci/android_arm64-cross-file.ini _android_build
meson compile -C _android_build
```

### Installation

In order to be executed the executable must be placed in a directory that isn't mounted with `noexec`.

The devices `/` or `/system/` partition can be read/write:
```bash
adb -d root
adb -d shell mount -oremount,rw /
```

```bash
adb -d root
adb -d shell mkdir /system_ext/bin/fwupd/
for f in \
 fwupd-poc-binder \
 gbinder.conf \
 subprojects/libgbinder/libgbinder.so \
 subprojects/libglibutil/libglibutil.so \
 subprojects/glib-2.78.3/glib/libglib-2.0.so \
 subprojects/glib-2.78.3/gobject/libgobject-2.0.so \
 subprojects/libffi/src/libffi.so \
 subprojects/proxy-libintl/libintl.so \
 ; do 
  adb -d push _android_build/$f /system_ext/bin/fwupd/
done
adb -d shell 'LD_LIBRARY_PATH=/system_ext/bin/fwupd/ GBINDER_CONFIG_FILE=/system_ext/bin/fwupd/gbinder.conf /system_ext/bin/fwupd/fwupd-poc-binder'
```

Then you can check if the service has been registered with
```
adb -d shell service list | grep fwupd
```

Patches and `meson.build` files for subprojects are in `subprojects/packagefiles`

## Client

In order for a client to connect out binder service we must either tag the executable to allow it to expose a service or switch off selinux:

```bash
adb -d shell setenforce 'permissive'
```

The selinux error looks is:
```
SELinux : avc:  denied  { find } for pid=24772 uid=10177 name=fwupd_poc scontext=u:r:untrusted_app:s0:c177,c256,c512,c768 tcontext=u:object_r:default_android_service:s0 tclass=service_manager permissive=0
```

I have tried `chcon u:object_r:default_android_service:s0 fwupd-poc-binder` but it doesn't fix it.

The clients output can be viewed with `adb -d logcat` or filtered with:
```bash
adb -d logcat 'fwupd_client:*' 'op.fwupd.client:*' 'AndroidRuntime:*' 'TransactionExecutor:*' 'SELinux:*' '*:S'
```

The app can be installed with
```bash
./gradlew installDebug
```