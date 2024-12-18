package org.freedesktop.fwupd.client

import android.os.BaseBundle
import android.os.PersistableBundle
import android.os.ParcelFileDescriptor
import android.os.Bundle
import android.os.IBinder
import android.content.ContentResolver
import android.content.Intent
import android.content.Context
import android.content.ServiceConnection
import android.content.ComponentName
import android.net.LocalServerSocket
import android.net.LocalSocketAddress
import android.net.LocalSocket
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.Text
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ButtonDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.activity.compose.setContent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.result.launch
import android.util.Log
import androidx.appcompat.app.AppCompatActivity
import androidx.activity.ComponentActivity
import org.freedesktop.fwupdpoc.IPocFwupd
import org.freedesktop.fwupdpoc.IPocFwupdListener
import org.freedesktop.fwupdpoc.Device
import org.freedesktop.fwupdpoc.DeepExample
import org.freedesktop.fwupd.MainService
import org.freedesktop.fwupd.IFwupd
import org.freedesktop.fwupd.IFwupdEventListener
import kotlin.collections.toTypedArray
import java.io.FileDescriptor

const val TAG = "fwupd_client"

const val FWUPD_POC_SERVICE = "fwupd_poc"
const val FWUPD_SERVICE = "fwupd"

//var logText: String by mutableStateOf("")

fun printBundle(bundle: BaseBundle?): String {
    var out = ""//"$bundle"uu
    if (bundle != null) {
        for (key in bundle.keySet()) {
            val v = bundle.get(key)

            val a = when (v) {
                is BooleanArray -> v.toTypedArray()
                is IntArray -> v.toTypedArray()
                is LongArray -> v.toTypedArray()
                is DoubleArray -> v.toTypedArray()
                else -> v
            }

            if (a is Array<*>) {
                out += "\n$key: ["
                for (s in a) {
                    out += "\n  ${s},"
                }
                out += "\n],"
            }
            else if (a is BaseBundle) {
                out += "\n$key: ${printBundle(a)},"
            }
            else {
                out += "\n$key: ${a},"
            }
        }
        return "$bundle:{${out.prependIndent("  ")}\n}"
    } else {
        return "(null)"
    }
}
fun printArray(array: Array<*>?): String {
    var out = ""

    if (array != null)
        out = (array.joinToString(prefix="[\n", postfix = ",\n]", separator = ",\n") {
            (
                when (it) {
                    is BaseBundle -> printBundle(it)
                    is Array<*> -> printArray(it)
                    else -> it.toString()
                }
            ).prependIndent("  ")
        })
    else
        out = "(null)"

    return out
}

fun getPocFwupdService(): IPocFwupd? {
    // Create hidden class ServiceManager and connect to binder service
    var binder = Class.forName("android.os.ServiceManager")
        .getDeclaredMethod("getService", String::class.java)
        .invoke(null, FWUPD_POC_SERVICE) as IBinder?
    // Wrap binder service in aidl interface
    return IPocFwupd.Stub.asInterface(binder)
}

fun getFwupdService(): IFwupd? {
    // Create hidden class ServiceManager and connect to binder service
    var binder = Class.forName("android.os.ServiceManager")
        .getDeclaredMethod("getService", String::class.java)
        .invoke(null, FWUPD_SERVICE) as IBinder?
    // Wrap binder service in aidl interface
    return IFwupd.Stub.asInterface(binder)
}

class MainActivity : ComponentActivity() {
    private var mPocService: IPocFwupd? = null
    private var mFwupdService: IFwupd? = null
    //val listeners = mutableListOf<IFwupdListener>()

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(className: ComponentName, service: IBinder) {
            //IPocFwupd.Stub.asInterface(service)
            mPocService = IPocFwupd.Stub.asInterface(service)
            w("app service = $mPocService")
        }
        override fun onServiceDisconnected(className: ComponentName) {
            w("service disco")
            mPocService = null
        }
    }

    private fun log(message: String) {
        logText = "$message\n\n$logText"
    }
    private fun v(message: String) {
        Log.v(TAG, message)
        log(message)
    }
    private fun w(message: String) {
        Log.w(TAG, message)
        log(message)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        w("Starting fwupd_poc client")


        setContent {
            MainView(null)
        }

    }

    fun connectToSystemService(serviceId: String?) {
        when (serviceId) {
            FWUPD_POC_SERVICE -> {
                mPocService = getSystemService(FWUPD_POC_SERVICE) as IPocFwupd? ?: getPocFwupdService()
                w("$serviceId = $mPocService")

                if (mPocService != null)
                    setContent {
                        MainView(serviceId)
                    }
            }
            FWUPD_SERVICE -> {
                mFwupdService = getSystemService(FWUPD_SERVICE) as IFwupd? ?: getFwupdService()
                w("$serviceId = $mFwupdService")

                if (mFwupdService != null)
                    setContent {
                        MainView(serviceId)
                    }
            }
        }

    }

    fun connectToAppService() {
        Intent(this, MainService::class.java).also { intent ->
            startService(intent)
            if (bindService(intent, connection, Context.BIND_AUTO_CREATE)) {
                w("binding app service")
            } else {
                w("failed to bind app service")
            }

            setContent {
                MainView(FWUPD_POC_SERVICE)
            }
        }

    }

    fun registerFwupdListener() {
        val listener = object : IFwupdEventListener.Stub() {
            override fun onChanged() {
                v("listener changed")
            }
            override fun onDeviceAdded(device: PersistableBundle?) {
                v("""
                |device added
                |  ${printBundle(device)}
                """.trimMargin())
            }
            override fun onDeviceRemoved(device: PersistableBundle?) {
                v("""
                |device removed
                |  ${printBundle(device)}
                """.trimMargin())
            }
            override fun onDeviceChanged(device: PersistableBundle?) {
                v("""
                |device changed
                |  ${printBundle(device)}
                """.trimMargin())
            }
            override fun onDeviceRequest(request: PersistableBundle?) {
                v("""
                |device request
                |  ${printBundle(request)}
                """.trimMargin())
            }
        }
        mFwupdService?.addEventListener(listener)
    }

    fun registerListener() {
        val listener = object : IPocFwupdListener.Stub() {
            override fun onChanged() {
                v("listener changed")
            }
            override fun onDeviceAdded(device: Device) {
                v("""
                    |listener device added
                    |  $device
                    |    id: ${device.id}
                    |    ido: ${device.ido}
                    |    path: ${device.path}
                    """.trimMargin())
            }
            override fun onDeepExample(de: DeepExample) {
                v("""
                    |listener deep example
                    |  $de
                    |    other: ${de.other}
                    |      id: ${de.other?.id}
                    |      example: ${de.other?.example}
                    |    id: ${de.id}
                    |    example: ${de.example}
                    """.trimMargin())
            }
            override fun onParams(int_param: Int, double_param: Double, string_param: String?, bool_param: Boolean) {
                v("""
                    |listener params
                    |  $int_param, $double_param, $string_param, $bool_param
                    """.trimMargin())
            }
            override fun onBundles(count: Int, bundles: Array<PersistableBundle?>?, status: String?) {
                var bundlesString = printArray(bundles)
                /*
                if (bundles != null) {
                    for (bundle in bundles) {
                        bundlesString += bundle.toString()
                    }
                }
                */
                v("""
                    |listener bundles
                    |  $count,
                    |${bundlesString.prependIndent("  ")},
                    |  $status,
                    """.trimMargin())
            }
        }
        mPocService?.addListener(listener)

    }


    @Composable
    fun MainView(serviceId: String?) {

        val startInstallResult =
        rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { resultUri ->
            if (resultUri != null) {
                v("${resultUri}")
                var pfd = contentResolver.openFileDescriptor(resultUri, "r")

                v("${pfd} ${pfd?.getFileDescriptor()?.valid()}")

                var guid = "This is a long and cool guid, the sort of guid you really identify with"
                var options = PersistableBundle()
                options.putString("hello", "world")
                options.putInt("value", 42)
                var other_fd = mFwupdService?.install(guid, pfd, options)
                v("${other_fd}")
                //mFwupdService?.install(pfd)
            }
        }

        val startPocSetFdResult =
        rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { resultUri ->
            if (resultUri != null) {
                v("${resultUri}")
                var pfd = contentResolver.openFileDescriptor(resultUri, "r")

                v("${pfd} ${pfd?.getFileDescriptor()?.valid()}")
                mPocService?.setFD(pfd)
            }
        }


        Column {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                Button(
                    onClick = {
                        connectToAppService()
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF008F8F))
                ) {
                    Text("bind app PoC svc")
                }
                Button(
                    onClick = {
                        connectToSystemService(FWUPD_POC_SERVICE)
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF008F8F))
                ) {
                    Text("bind sys PoC svc")
                }
                Button(
                    onClick = {
                        connectToSystemService(FWUPD_SERVICE)
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF008F8F))
                ) {
                    Text("bind sys fwupd svc")
                }
            }
            when (serviceId) {
                FWUPD_SERVICE -> Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                    Button(
                        onClick = {
                            registerFwupdListener()
                            v("register listener")
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF8F8F00))
                    ) {
                        Text("register evt listener")
                    }
                    Button(onClick = {
                        var bundleOut = mFwupdService?.getDevices()
                        var vString = "getDevices returned "
                        vString += printArray(bundleOut)
                        v(vString)
                        //v(bundleOut?.first().toString())
                    }) {
                        Text("getDevices")
                    }
                    Button(onClick = {
                        v("install")
                        var uri = startInstallResult.launch(arrayOf<String>("application/x-cab"))
                    }) {
                        Text("install")
                    }
                }
                FWUPD_POC_SERVICE -> Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                    Button(
                        onClick = {
                            registerListener()
                            v("register listener ")
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF8F8F00))
                    ) {
                        Text("register evt listener")
                    }
                    Button(
                        onClick = {
                            v("call triggerChanged")
                            mPocService?.triggerChange()
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFA83F42))
                    ) {
                        Text("trigger onChange evt")
                    }
                    Button(
                        onClick = {
                            v("call triggerDeviceAdded")
                            mPocService?.triggerDeviceAdded()
                        },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFA83F42))
                    ) {
                        Text("trigger onDeviceAdded evt")
                    }
                    Button(onClick = {
                        v("setFd")
                        var uri = startPocSetFdResult.launch(arrayOf<String>("application/x-cab"))
                    }) {
                        Text("setFd")
                    }
                    Button(onClick = {
                        var bundleOut = mPocService?.getBundles()
                        var vString = "getBundles returned "
                        vString += printArray(bundleOut)
                        v(vString)
                    }) {
                        Text("getBundles")
                    }
                    Button(onClick = {
                        var bundleOut = mPocService?.getBundleMatrix()
                        var vString = "getBundleMatrix returned "
                        vString += printArray(bundleOut)
                        v(vString)
                    }) {
                        Text("getBunMat")
                    }
                    Button(onClick = {
                        var intArray = mPocService?.getIntArray()
                        var vString = "getIntArray returned "
                        vString += printArray(intArray?.toTypedArray())
                        v(vString)
                    }) {
                        Text("getIntArray")
                    }
                    Button(onClick = {
                        var stringArray = mPocService?.getStrings()
                        var vString = "getStrings returned "
                        vString += printArray(stringArray)
                        v(vString)
                    }) {
                        Text("getStrs")
                    }
                    Button(onClick = {
                        var stringArray = mPocService?.getMaybeStrings()
                        var vString = "getMaybeStrings returned "
                        vString += printArray(stringArray)
                        v(vString)
                    }) {
                        Text("getMaybeStrs")
                    }
                    Button(onClick = {
                        var boolArray = mPocService?.getBools()
                        var vString = "getBools returned "
                        vString += printArray(boolArray?.toTypedArray())
                        v(vString)
                    }) {
                        Text("getBools")
                    }
                    Button(onClick = {
                        var bundleOut = mPocService?.getBundle()
                        var vString = "getBundles returned "
                        vString += printBundle(bundleOut)
                        v(vString)
                    }) {
                        Text("getBundle")
                    }
                    Button(onClick = {
                        var stringOut = mPocService?.getString()
                        v("getString returned $stringOut")
                    }) {
                        Text("getString")
                    }
                    Button(onClick = {
                        var stringOut = mPocService?.setString("test string")
                        v("setString sent $stringOut")
                    }) {
                        Text("setString")
                    }
                    Button(onClick = {
                        var fdOut = mPocService?.getInt()
                        v("getInt returned $fdOut")
                    }) {
                        Text("getInt")
                    }
                    Button(onClick = {
                        var fdOut = mPocService?.setInt(42)
                        v("setInt returned $fdOut")
                    }) {
                        Text("setInt")
                    }
                }
                else -> {}
            }
            Card(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).horizontalScroll(rememberScrollState())) {
                Column(Modifier.fillMaxSize()) {
                    Text(logText, Modifier.padding(3.dp), fontFamily = FontFamily.Monospace)
                }
            }
        }
    }
}
