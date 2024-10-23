package org.freedesktop.fwupd.client

import org.json.JSONObject
import android.os.Bundle
import android.os.IBinder
import android.content.Intent
import android.content.Context
import android.content.BroadcastReceiver
import android.content.IntentFilter
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.border
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
import androidx.core.content.IntentCompat
import androidx.core.content.ContextCompat
import android.util.Log
import android.hardware.usb.UsbManager
import android.hardware.usb.UsbDevice
import androidx.appcompat.app.AppCompatActivity
import androidx.activity.ComponentActivity
import org.freedesktop.fwupd.IPocFwupd

//const val TAG = "fwupd_client"
var logText: String by mutableStateOf("")

@kotlin.ExperimentalStdlibApi
class UsbActivity : ComponentActivity() {
    private var mService: UsbManager? = null
    //private var logText: String by mutableStateOf("")
    private var usbReceiver: BroadcastReceiver? = null

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

    private fun deviceToString(device: UsbDevice): String {
        return """
            |${device.getDeviceName()}
            |    ${String.format("%04x:%04x", device.getVendorId(), device.getProductId())}
            |    ${device.getManufacturerName()}
            |    ${device.getProductName()}
            """.trimMargin()
    }

		override fun onDestroy() {
        w(" - - - Stopping fwupd_usb_poc client - - - ")
        super.onDestroy()
    }
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        w(" - - - Starting fwupd_usb_poc client - - - ")
        mService = getSystemService(Context.USB_SERVICE) as UsbManager?
        w("usb = $mService")


        usbReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                when (intent.action) {
                    UsbManager.ACTION_USB_DEVICE_ATTACHED -> {
                        val device: UsbDevice? = IntentCompat.getParcelableExtra(intent, UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                        device?.apply {
                            v("+ ${deviceToString(device)}")
                        }
                    }
                    UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                        val device: UsbDevice? = IntentCompat.getParcelableExtra(intent, UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                        device?.apply {
                            v("- ${deviceToString(device)}")
                        }
                    }
                }
            }
        }
        val intentFilter = IntentFilter()
        intentFilter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
        intentFilter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        ContextCompat.registerReceiver(this, usbReceiver, intentFilter, ContextCompat.RECEIVER_NOT_EXPORTED)
        w("usb broadcast receiver = $usbReceiver")

        setContent {
            MainView()
        }

    }

    @Composable
    fun MainView() {
        Column {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = {
                    val devices = mService?.getDeviceList()
                    var message = "getDeviceList = [\n"
                    devices?.forEach { (_, device) -> message += "  ${deviceToString(device)}\n" }
                    v("$message\n]")
                }) {
                    Text("getDeviceList")
                }
                Button(onClick = {
                    val devices = mService?.getAccessoryList()
                    var message = "getAccessoryList = [\n"
                    devices?.forEach { accessory ->
                        message += "  $accessory\n"
                    }
                    v("$message\n]")
                }) {
                    Text("getAccessoryList")
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
