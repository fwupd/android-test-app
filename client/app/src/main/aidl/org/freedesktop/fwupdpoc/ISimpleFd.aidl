package org.freedesktop.fwupdpoc;

interface ISimpleFd {
    void sendFd(in String str, in ParcelFileDescriptor fd, in PersistableBundle bundle);
}