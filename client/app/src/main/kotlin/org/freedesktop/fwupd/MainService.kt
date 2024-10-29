package org.freedesktop.fwupd

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.os.ParcelFileDescriptor
import org.freedesktop.fwupd.IPocFwupd
import org.freedesktop.fwupd.IFwupdListener

class MainService : Service() {
    val listeners = mutableListOf<IFwupdListener>()

    override fun onCreate()
    {
        super.onCreate()
    }

    override fun onBind(intent: Intent): IBinder {
        return binder
    }

    private val binder = object : IPocFwupd.Stub() {
        override fun getString(): String? {
            return "hello world"
        }
        override fun setString(value: String?) {

        }
        override fun getFD(): ParcelFileDescriptor? {
            return null
        }
        override fun setFD(value: ParcelFileDescriptor?) {

        }
        override fun getDict() {}
        override fun setDict() {}
        override fun getInt(): Int {
            return 42
        }
        override fun setInt(value: Int) {

        }
        override fun getIntArray(): IntArray {
            return intArrayOf(23, 23)
        }
        //override fun setIntArray(value: Array<int>?) {

        //}
        override fun addListener(listener: IFwupdListener) {
            listeners.add(listener)

        }
        override fun removeListener(listener: IFwupdListener) {

        }
        override fun triggerChange() {
            for (listener in listeners) {
                listener.onChanged()
            }
        }


    }
}