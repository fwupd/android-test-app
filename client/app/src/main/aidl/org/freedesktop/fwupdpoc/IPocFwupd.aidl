package org.freedesktop.fwupdpoc;

import org.freedesktop.fwupdpoc.IPocFwupdListener;

interface IPocFwupd {
    String getString();
    void setString(String value);
    ParcelFileDescriptor getFD();
    void setFD(in ParcelFileDescriptor value);
    // HashMap??? getDict();
    void getDict();
    // void setDict(HashMap???);
    void setDict();
    int getInt();
    void setInt(int value);
    int[] getIntArray();
    //void setIntArray(int[] value);
    PersistableBundle getBundle();
    PersistableBundle[] getBundles();
    PersistableBundle[2][2] getBundleMatrix();
    String[] getStrings();
    boolean[] getBools();
    String[] getMaybeStrings();
    void addListener(IPocFwupdListener listener);
    void removeListener(IPocFwupdListener listener);
    void triggerChange();
    void triggerDeviceAdded();
}
