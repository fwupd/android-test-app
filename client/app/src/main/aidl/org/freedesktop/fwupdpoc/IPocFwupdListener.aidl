package org.freedesktop.fwupdpoc;

import org.freedesktop.fwupdpoc.Device;
import org.freedesktop.fwupdpoc.DeepExample;

interface IPocFwupdListener {
    void onChanged();
    void onDeviceAdded(in Device device);
    void onDeepExample(in DeepExample de);
    void onParams(int int_param, double double_param, String string_param, boolean bool_param);
    void onBundles(int count, in @nullable PersistableBundle[] bundles, String status);
}
