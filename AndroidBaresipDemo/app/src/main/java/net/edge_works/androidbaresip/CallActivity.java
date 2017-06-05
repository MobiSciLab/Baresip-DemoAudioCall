package net.edge_works.androidbaresip;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.support.v4.app.FragmentTransaction;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.TextView;

import net.edge_works.baresiplib.TelephonyEvent;
import net.edge_works.baresiplib.TelephonyService;


/**
 * Created by caominhvu on 3/28/17.
 */

public class CallActivity extends AppCompatActivity {

    LocalBroadcastManager mLocalBroadcastManager;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_call);

        if(!getIntent().getBooleanExtra(TelephonyService.IS_INCOMING, false)) {
            findViewById(R.id.ll_decline_answer).setVisibility(View.GONE);
            findViewById(R.id.btn_hangup).setVisibility(View.VISIBLE);
        }
        ((TextView) findViewById(R.id.tv_status)).setText(getIntent().getBooleanExtra(TelephonyService.IS_INCOMING, false)? "Incoming Call": "Outgoing Call");
        ((TextView) findViewById(R.id.tv_contact_name)).setText(getIntent().getStringExtra(TelephonyService.PEER_NAME));


        IntentFilter filter = new IntentFilter();
        filter.addAction(TelephonyService.ACTION_CALL_ESTABLISHED);
        filter.addAction(TelephonyService.ACTION_CALL_CLOSE);
        filter.addAction(TelephonyService.ACTION_TELEPHONY_EVENT);
        filter.addAction(TelephonyService.ACTION_CALL_DURATION);
        mLocalBroadcastManager = LocalBroadcastManager.getInstance(CallActivity.this);
        mLocalBroadcastManager.registerReceiver(mBroadcastReceiver, filter);
    }

    BroadcastReceiver mBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, final Intent intent) {
            String action = intent == null? null: intent.getAction();
            if (TelephonyService.ACTION_CALL_ESTABLISHED.equals(action)) {
            } else if(TelephonyService.ACTION_CALL_CLOSE.equals(action)) {
                finish();
            } else if(TelephonyService.ACTION_CALL_DURATION.equals(action)) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        long duration = intent.getLongExtra(TelephonyService.DURATION, 0);
                        ((TextView) findViewById(R.id.tv_status)).setText(String.format("%02d:%02d:%02d", duration / 3600,
                                (duration % 3600) / 60, (duration % 60)));
                    }
                });
            } else{
                int event = intent.getIntExtra(TelephonyService.TELEPHONY_EVENT, -1);
                if (event != -1) {
                    ((TextView) findViewById(R.id.tv_status)).setText(TelephonyEvent.toString(event));
                }
            }
        }
    };

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if(mLocalBroadcastManager != null) {
            mLocalBroadcastManager.unregisterReceiver(mBroadcastReceiver);
        }
    }

    public void onClickDecline(View view) {
        Intent intent = new Intent(CallActivity.this, TelephonyService.class);
        intent.setAction(TelephonyService.ACTION_DECLINE);
        startService(intent);
        finish();
    }

    public void onClickHangup(View view) {

        Intent intent = new Intent(CallActivity.this, TelephonyService.class);
        intent.setAction(TelephonyService.ACTION_DECLINE);
        startService(intent);

        finish();
    }

    public void onClickAnswer(View view) {
        Intent intent = new Intent(CallActivity.this, TelephonyService.class);
        intent.setAction(TelephonyService.ACTION_ANSWER);
        startService(intent);

        findViewById(R.id.ll_decline_answer).setVisibility(View.GONE);
        findViewById(R.id.btn_hangup).setVisibility(View.VISIBLE);
    }

    boolean isAudioMuted = false;


    public void onClickMuteAudio(View view) {
        isAudioMuted = !isAudioMuted;

        Bundle bundle = new Bundle();
        bundle.putBoolean(TelephonyService.IS_MUTED, isAudioMuted);
        Intent intent = new Intent(CallActivity.this, TelephonyService.class);
        intent.setAction(TelephonyService.ACTION_MUTE_AUDIO);
        intent.putExtras(bundle);
        startService(intent);
    }
}
