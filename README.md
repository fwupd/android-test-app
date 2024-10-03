The devices `/` or `/system/` partition must be read write
```bash
adb -d root
adb -d shell mount -oremount,rw /
```

Setup the NDK path in `android_aarch64_cross_file.ini` and run
```bash
meson setup --cross-file ../fwupd/contrib/ci/android_arm64-cross-file.ini _android_build
meson compile -C _android_build

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

