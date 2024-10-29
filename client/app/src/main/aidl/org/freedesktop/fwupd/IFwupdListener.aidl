package org.freedesktop.fwupd;

interface IFwupdListener {
    void onChanged();
    void onDeviceAdded();
}
