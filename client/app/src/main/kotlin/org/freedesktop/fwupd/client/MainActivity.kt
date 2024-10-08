package org.freedesktop.fwupd.client

import android.os.Bundle
import android.os.IBinder
import android.content.Intent
import android.content.Context
import androidx.compose.foundation.layout.Column
import androidx.compose.material3.Text
import androidx.compose.material3.Button
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.activity.compose.setContent
import android.util.Log
import androidx.appcompat.app.AppCompatActivity
import androidx.activity.ComponentActivity
import org.freedesktop.fwupd.IPocFwupd

const val TAG = "fwupd_client"

const val FWUPD_SERVICE = "fwupd_poc"

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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        Log.w(TAG, " - - - - Starting fwupd_poc client")
        mService = getSystemService(FWUPD_SERVICE) as IPocFwupd? ?: getFwupdService()
        Log.w(TAG, " - - - - fwupd_poc = $mService")

        setContent {
            MainView()
        }

    }

    @Composable
    fun MainView() {
        Column {
            Text("fwupd poc")
            Button(onClick = {
                var stringOut = mService?.getString()
                Log.w(TAG, " - - - - - getString returned $stringOut")
            }) {
                Text("getString")
            }
            Button(onClick = {
                var stringOut = mService?.setString("test string")
                Log.w(TAG, " - - - - - setString sent $stringOut")
            }) {
                Text("setString")
            }
            Button(onClick = {
                var fdOut = mService?.getInt()
                Log.w(TAG, " - - - - - getInt returned $fdOut")
            }) {
                Text("getInt")
            }
            Button(onClick = {
                var fdOut = mService?.setInt(42)
                Log.w(TAG, " - - - - - setInt returned $fdOut")
            }) {
                Text("setInt")
            }
        }
    }
}
