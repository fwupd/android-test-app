package org.freedesktop.fwupd.client

import android.os.Bundle
import android.os.IBinder
import android.content.Intent
import android.content.Context
import android.content.ServiceConnection
import android.content.ComponentName
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
import android.util.Log
import androidx.appcompat.app.AppCompatActivity
import androidx.activity.ComponentActivity
import org.freedesktop.fwupd.IPocFwupd
import org.freedesktop.fwupd.IFwupdListener
import org.freedesktop.fwupd.MainService

const val TAG = "fwupd_client"

const val FWUPD_SERVICE = "fwupd_poc"

//var logText: String by mutableStateOf("")

fun getFwupdService(): IPocFwupd? {
    // Create hidden class ServiceManager and connect to binder service
    var binder = Class.forName("android.os.ServiceManager")
        .getDeclaredMethod("getService", String::class.java)
        .invoke(null, FWUPD_SERVICE) as IBinder?
    // Wrap binder service in aidl interface
    return IPocFwupd.Stub.asInterface(binder)
}

class MainActivity : ComponentActivity() {
    private var mService: IPocFwupd? = null
    //val listeners = mutableListOf<IFwupdListener>()

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(className: ComponentName, service: IBinder) {
            //IPocFwupd.Stub.asInterface(service)
            mService = IPocFwupd.Stub.asInterface(service)
            w("app service = $mService")
        }
        override fun onServiceDisconnected(className: ComponentName) {
            w("service disco")
            mService = null
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
            MainView()
        }

    }

    fun connectToSystemService() {
        mService = getSystemService(FWUPD_SERVICE) as IPocFwupd? ?: getFwupdService()
        w("fwupd_poc = $mService")
    }

    fun connectToAppService() {
        Intent(this, MainService::class.java).also { intent ->
            startService(intent)
            if (bindService(intent, connection, Context.BIND_AUTO_CREATE)) {
                w("binding app service")
            } else {
                w("failed to bind app service")
            }
        }
    }

    fun registerListener() {
        val listener = object : IFwupdListener.Stub() {
            override fun onChanged() {
                v("listener changed")
            }
            override fun onDeviceAdded() {
                v("listener device added")
            }
        } 
        mService?.addListener(listener)
        //listeners.add(listener)

    }


    @Composable
    fun MainView() {
        Column {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = {
                    connectToAppService()
                }) {
                    Text("bind app service")
                }
                Button(onClick = {
                    connectToSystemService()
                }) {
                    Text("bind system service")
                }
                Button(onClick = {
                    registerListener()
                    v("register listener ")
                }) {
                    Text("register listener")
                }
                Button(onClick = {
                    //var stringOut = mService?.getString()
                    // TODO: create IFwupdListener
                    v("call triggerChanged")
                    mService?.triggerChange()
                }) {
                    Text("trigger listener")
                }
                Button(onClick = {
                    var stringOut = mService?.getString()
                    v("getString returned $stringOut")
                }) {
                    Text("getString")
                }
                Button(onClick = {
                    var stringOut = mService?.setString("test string")
                    v("setString sent $stringOut")
                }) {
                    Text("setString")
                }
                Button(onClick = {
                    var fdOut = mService?.getInt()
                    v("getInt returned $fdOut")
                }) {
                    Text("getInt")
                }
                Button(onClick = {
                    var fdOut = mService?.setInt(42)
                    v("setInt returned $fdOut")
                }) {
                    Text("setInt")
                }
            }
            Card(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).horizontalScroll(rememberScrollState())) {
                Column(Modifier.fillMaxSize()) {
                    Text(logText, Modifier.padding(8.dp), fontFamily = FontFamily.Monospace)
                }
            }
        }
    }
}
