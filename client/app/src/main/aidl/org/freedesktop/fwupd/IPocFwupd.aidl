package org.freedesktop.fwupd;

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
}