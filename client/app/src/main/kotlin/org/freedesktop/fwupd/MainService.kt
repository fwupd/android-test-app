package org.freedesktop.fwupd

import android.app.Service
import android.content.Intent
import android.os.Bundle
import android.os.PersistableBundle
import android.os.IBinder
import android.os.ParcelFileDescriptor
import org.freedesktop.fwupdpoc.IPocFwupd
import org.freedesktop.fwupdpoc.IPocFwupdListener
import org.freedesktop.fwupdpoc.Device
import org.freedesktop.fwupdpoc.OtherExample
import org.freedesktop.fwupdpoc.DeepExample
import org.freedesktop.fwupd.IFwupd

class MainService : Service() {
    val listeners = mutableListOf<IPocFwupdListener>()

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
        override fun getBundle(): PersistableBundle {
            val bundle = PersistableBundle()
            bundle.putBoolean("yolo", true)
            bundle.putString("hi", "value")
            return bundle
        }
        override fun getBundles(): Array<PersistableBundle> {
            val bundle = PersistableBundle()
            bundle.putBoolean("yolo", true)
            bundle.putString("hi", "value")
            val bundle_2 = PersistableBundle()
            bundle_2.putDouble("yolo", 1.0)
            bundle_2.putString("hi", "hehe")
            return arrayOf(bundle, bundle_2)
        }
        override fun getBundleMatrix(): Array<Array<PersistableBundle>> {
            val bundle = PersistableBundle()
            bundle.putBoolean("boolValue", true)
            bundle.putString("stringValue", "value")
             return arrayOf(arrayOf(bundle.deepCopy(), bundle.deepCopy()),
                arrayOf(bundle.deepCopy(), bundle.deepCopy()))
        }
        override fun getStrings(): Array<String?> {
            return arrayOf("this", "that", "other", null)
        }
        override fun getBools(): BooleanArray {
            return booleanArrayOf(true, false, false, true, true, false, false, true, true)
        }
        override fun getMaybeStrings(): Array<String?> {
            return arrayOf("this", "that", "other")
        }
        override fun addListener(listener: IPocFwupdListener) {
            listeners.add(listener)

        }
        override fun removeListener(listener: IPocFwupdListener) {

        }
        override fun triggerChange() {
            for (listener in listeners) {
                listener.onChanged()
                listener.onParams(1, 0.1, "string param", true)
                listener.onParams(1, 0.1, null, true)
                val pb = PersistableBundle()
                pb.putString("hello", "world")
                val pbs = arrayOf(pb, pb)
                listener.onBundles(pbs.size, pbs, "Okay")
            }
        }
        override fun triggerDeviceAdded() {
            for (listener in listeners) {
                val device = Device()
                device.id = 24
                device.ido = 464
                device.path = "hello"
                listener.onDeviceAdded(device)

                val oe = OtherExample()
                val de = DeepExample()
                de.other = oe
                oe.example = "other example"
                oe.id = 31
                de.example = "example"
                de.id = 32
                listener.onDeepExample(de)
            }
        }


    }
}