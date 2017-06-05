package net.edge_works.androidbaresip;

import android.Manifest;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import net.edge_works.baresiplib.TelephonyEvent;
import net.edge_works.baresiplib.TelephonyService;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.

    EditText mEdtAgentName, mEdtAgentPassword, mEdtSipServer, mEdtICEServer;
    LocalBroadcastManager mLocalBroadcastManager;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mEdtAgentName = (EditText) findViewById(R.id.edt_agent_name);
        mEdtAgentPassword = (EditText) findViewById(R.id.edt_agent_password);
        mEdtSipServer = (EditText) findViewById(R.id.edt_sip_server);
        mEdtICEServer = (EditText) findViewById(R.id.edt_ice_server);

        Bundle bundle = new Bundle();
        bundle.putSerializable(TelephonyService.ACTIVITY_CLASS, CallActivity.class);
        triggerTelephonnyService(TelephonyService.ACTION_INIT, bundle);

        IntentFilter filter = new IntentFilter();
        filter.addAction(TelephonyService.ACTION_TELEPHONY_EVENT);
        mLocalBroadcastManager = LocalBroadcastManager.getInstance(MainActivity.this);
        mLocalBroadcastManager.registerReceiver(mTelephonyEventReceiver, filter);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (ContextCompat.checkSelfPermission(MainActivity.this,
                Manifest.permission.RECORD_AUDIO)
                != PackageManager.PERMISSION_GRANTED) {

            ActivityCompat.requestPermissions(MainActivity.this,
                    new String[]{Manifest.permission.CAPTURE_AUDIO_OUTPUT, Manifest.permission.MODIFY_AUDIO_SETTINGS, Manifest.permission.RECORD_AUDIO},
                    0);
        }
    }

    public void onClickCreateUserAgent(View view) {
        Bundle bundle = new Bundle();
        bundle.putString(TelephonyService.AGENT_NAME, mEdtAgentName.getText().toString());
        bundle.putString(TelephonyService.AGENT_PASSWORD, mEdtAgentPassword.getText().toString());
        bundle.putString(TelephonyService.SIP_SERVER, mEdtSipServer.getText().toString());
        bundle.putString(TelephonyService.ICE_SERVER, mEdtICEServer.getText().toString());

        triggerTelephonnyService(TelephonyService.ACTION_CREATE_USERAGENT, bundle);
    }

    public void onClickStartAudioCall(View view) {
        String contact = ((EditText) findViewById(R.id.edt_contact)).getText().toString();

        Bundle bundle = new Bundle();
        bundle.putString(TelephonyService.CONTACT, contact);

        triggerTelephonnyService(TelephonyService.ACTION_START_AUDIO_CALL, bundle);
    }

    public void onClickVideoCall(View view) {
        Intent incomingCall = new Intent(MainActivity.this, CallActivity.class);
        incomingCall.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        incomingCall.putExtra(TelephonyService.IS_VIDEO, true);
        startActivity(incomingCall);

        String contact = ((EditText) findViewById(R.id.edt_contact)).getText().toString();
        Bundle bundle = new Bundle();
        bundle.putString(TelephonyService.CONTACT, contact);
        triggerTelephonnyService(TelephonyService.ACTION_START_VIDEO_CALL, bundle);
    }

    private void triggerTelephonnyService(String action, Bundle bundle) {
        Intent intent = new Intent(MainActivity.this, TelephonyService.class);
        intent.setAction(action);
        if(bundle != null) {
            intent.putExtras(bundle);
        }
        startService(intent);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return super.onTouchEvent(event);
    }

    private BroadcastReceiver mTelephonyEventReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            int event = intent.getIntExtra(TelephonyService.TELEPHONY_EVENT, -1);
            if (event != -1) {
                ((TextView) findViewById(R.id.tv_event)).setText(TelephonyEvent.toString(event));
            }
        }
    };

}
