package net.edge_works.baresiplib;

import android.app.Service;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Process;
import android.support.annotation.Keep;
import android.support.annotation.Nullable;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;
import android.widget.Toast;

import net.edge_works.baresiplib.util.IOUtils;

import static net.edge_works.baresiplib.TelephonyEvent.UA_EVENT_CALL_CLOSED;
import static net.edge_works.baresiplib.TelephonyEvent.UA_EVENT_CALL_ESTABLISHED;
import static net.edge_works.baresiplib.TelephonyEvent.UA_EVENT_CALL_INCOMING;
import static net.edge_works.baresiplib.TelephonyEvent.UA_EVENT_CALL_RINGING;
import static net.edge_works.baresiplib.TelephonyEvent.UA_EVENT_REGISTERING;
import static net.edge_works.baresiplib.TelephonyEvent.UA_EVENT_REGISTER_FAIL;
import static net.edge_works.baresiplib.TelephonyEvent.UA_EVENT_REGISTER_OK;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.ref.WeakReference;


/**
 * Created by caominhvu on 3/20/17.
 */

public class TelephonyService extends Service {
    public static final String ACTION_INIT = "net.edge_works.androidbaresip.TelephonyService.ACTION_INIT";
    public static final String ACTION_CREATE_USERAGENT = "net.edge_works.androidbaresip.TelephonyService.ACTION_CREATE_USERAGENT";
    public static final String ACTION_DECLINE = "net.edge_works.androidbaresip.TelephonyService.ACTION_DECLINE";
    public static final String ACTION_ANSWER = "net.edge_works.androidbaresip.TelephonyService.ACTION_ANSWER";

    public static final String ACTION_START_AUDIO_CALL = "net.edge_works.androidbaresip.TelephonyService.ACTION_START_AUDIO_CALL";
    public static final String ACTION_START_VIDEO_CALL = "net.edge_works.androidbaresip.TelephonyService.ACTION_START_VIDEO_CALL";

    public static final String ACTION_MUTE_VIDEO = "net.edge_works.androidbaresip.TelephonyService.ACTION_MUTE_VIDEO";
    public static final String ACTION_MUTE_AUDIO = "net.edge_works.androidbaresip.TelephonyService.ACTION_MUTE_AUDIO";

    public static final String BUNDLE_USER_AGENT="BUNDLE_USER_AGENT";
    public static final String AGENT_NAME = "AGENT_NAME";
    public static final String AGENT_PASSWORD = "AGENT_PASSWORD";
    public static final String SIP_SERVER = "SIP_SERVER";
    public static final String ICE_SERVER = "ICE_SERVER";
    public static final String ACTIVITY_CLASS = "ACTIVITY_CLASS";
    public static final String IS_MUTED = "IS_MUTED";

    public static final String CONTACT = "CONTACT";

    private HandlerThread mWorkerThread, mDaemonThread;

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        mLocalBroadcastManager = LocalBroadcastManager.getInstance(TelephonyService.this);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i("CMV", getFilesDir().getAbsolutePath());
        String action = intent == null? null: intent.getAction();
        if(ACTION_INIT.equals(action)) {
            mCallActivityClass = (Class) intent.getExtras().getSerializable(ACTIVITY_CLASS);
            if(mWorkerThread == null && mDaemonThread == null) {
                mWorkerThread = new HandlerThread("BaresipThread", Process.THREAD_PRIORITY_URGENT_DISPLAY);
                mWorkerThread.start();
                mHandler = new MyHandler(mWorkerThread.getLooper(), TelephonyService.this);

                mDaemonThread = new HandlerThread("DaemonBaresipThread", Process.THREAD_PRIORITY_URGENT_DISPLAY);
                mDaemonThread.start();
                mDaemonHandler = new MyHandler(mDaemonThread.getLooper(), TelephonyService.this);
                copyAssets();
            }
        } else if(ACTION_CREATE_USERAGENT.equals(action)) {
            Message msg = Message.obtain();
            msg.what = TASK_CREATE_AGENT;
            msg.setData(intent.getExtras());
            mHandler.sendMessage(msg);
        } else if(ACTION_DECLINE.equals(action)) {
            mHandler.sendEmptyMessage(TASK_DECLINE);
        } else if(ACTION_ANSWER.equals(action)) {
            mHandler.sendEmptyMessage(TASK_ANSWER);
        } else if(ACTION_START_AUDIO_CALL.equals(action)) {
            Message msg = Message.obtain();
            msg.what = TASK_START_AUDIO_CALL;
            msg.setData(intent.getExtras());
            mHandler.sendMessage(msg);
        } else if(ACTION_START_VIDEO_CALL.equals(action)) {
            Message msg = Message.obtain();
            msg.what = TASK_START_VIDEO_CALL;
            msg.setData(intent.getExtras());
            mHandler.sendMessage(msg);
        } else if(ACTION_MUTE_VIDEO.equals(action)) {
            Message msg = Message.obtain();
            msg.what = TASK_MUTE_VIDEO;
            msg.setData(intent.getExtras());
            mHandler.sendMessage(msg);
        } else if(ACTION_MUTE_AUDIO.equals(action)) {
            Message msg = Message.obtain();
            msg.what = TASK_MUTE_AUDIO;
            msg.setData(intent.getExtras());
            mHandler.sendMessage(msg);
        }
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        mHandler.removeMessages(TASK_SETUP_BARESIP);
        mDaemonHandler.removeMessages(TASK_START_DAEMON);

        mWorkerThread.interrupt();
        mWorkerThread.quit();
        mDaemonThread.interrupt();
        mDaemonThread.quit();
    }

    private void copyAssets() {
        AssetManager assetManager = getAssets();
        String[] files = null;
        try {
            files = assetManager.list("baresip");
        } catch (IOException e) {
            e.printStackTrace();
        }
        if (files != null) {
            for (String filename : files) {
                Log.i("CMV", filename);
                InputStream in = null;
                OutputStream out = null;
                try {
                    in = assetManager.open("baresip/"  + filename);
                    File outFile = new File(getFilesDir().getPath()
                            , filename);
                    out = new FileOutputStream(outFile);
                    IOUtils.copy(in, out);
                } catch (IOException e) {
                    e.printStackTrace();
                } finally {
                    if (in != null) {
                        try {
                            in.close();
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                    }
                    if (out != null) {
                        try {
                            out.close();
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                    }
                }
            }
        }
        mHandler.sendEmptyMessage(TASK_SETUP_BARESIP);
    }

    static MyHandler mHandler, mDaemonHandler;

    private static final int TASK_SETUP_BARESIP = 0x0;
    private static final int TASK_START_DAEMON = 0x1;
    private static final int TASK_CREATE_AGENT = 0x2;
    private static final int TASK_ANSWER = 0x3;
    private static final int TASK_DECLINE = 0x4;

    private static final int TASK_START_AUDIO_CALL = 0x5;
    private static final int TASK_START_VIDEO_CALL = 0x6;
    private static final int TASK_GET_DURATION= 0x7;

    private static final int TASK_MUTE_VIDEO= 0x8;
    private static final int TASK_MUTE_AUDIO= 0x9;

    private static class MyHandler extends Handler {
        private final WeakReference<TelephonyService> mService;

        public MyHandler(Looper looper, TelephonyService activity) {
            super(looper);
            mService = new WeakReference<>(activity);
        }

        public void handleMessage(Message msg) {
            TelephonyService service = mService.get();
            if (service == null) return;
            switch (msg.what) {
                case TASK_SETUP_BARESIP:
                    Toast.makeText(mService.get(), "Resource is avai!", Toast.LENGTH_SHORT).show();
                    service.init(service.getFilesDir().getAbsolutePath());
                    mDaemonHandler.sendEmptyMessage(TASK_START_DAEMON);
                    break;
                case TASK_START_DAEMON:
                    service.startDaemon();
                    break;
                case TASK_CREATE_AGENT:
                    Bundle bundle = msg.getData();
                    service.setUserAgent(service.createAgent(bundle.getString(AGENT_NAME), bundle.getString(AGENT_PASSWORD), bundle.getString(SIP_SERVER), bundle.getString(ICE_SERVER)));
                    break;
                case TASK_ANSWER:
                    service.answer(service.mUserAgent);
                    break;
                case TASK_DECLINE:
                    service.decline(service.mUserAgent);
                    break;
                case TASK_START_AUDIO_CALL:
                    bundle = msg.getData();
                    service.start_audio_call(bundle.getString(CONTACT));
                    break;
                case TASK_START_VIDEO_CALL:
                    bundle = msg.getData();
                    service.start_video_call(bundle.getString(CONTACT));
                    break;
                case TASK_GET_DURATION:
                    Intent intent = new Intent(ACTION_CALL_DURATION);
                    intent.putExtra(DURATION, service.get_call_duration());
                    mLocalBroadcastManager.sendBroadcast(intent);
                    if(isCallActive) {
                        mHandler.sendEmptyMessageDelayed(TASK_GET_DURATION, 900);
                    }
                    break;


                case TASK_MUTE_AUDIO:
                    bundle = msg.getData();
                    service.mute_audio(bundle.getBoolean(IS_MUTED));
                    break;
            }
        }
    }

    static {
        System.loadLibrary("baresipwrapper");
    }
    private long mUserAgent=-1;
    public void setUserAgent(long userAgent) {
        mUserAgent = userAgent;
    }
    public native void init(String configPath);
    public native void startDaemon();
    public native long createAgent(String username, String password, String sipServer, String iceServer);
    public native void answer(long userAgent);
    public native void decline(long userAgent);
    public native void start_audio_call(String to);
    public native void start_video_call(String to);
    public native long get_call_duration();
    public native boolean is_video_call();
    public native void mute_audio(boolean isMuted);
    public native String get_peer_name();

    public static final String ACTION_TELEPHONY_EVENT = "net.edge_works.androidbaresip.TelephonyService.ACTION_TELEPHONY_EVENT";
    public static final String ACTION_CALL_ESTABLISHED = "net.edge_works.androidbaresip.TelephonyService.ACTION_CALL_ESTABLISHED";
    public static final String ACTION_CALL_CLOSE = "net.edge_works.androidbaresip.TelephonyService.ACTION_CALL_CLOSE";
    public static final String ACTION_CALL_DURATION = "net.edge_works.androidbaresip.TelephonyService.ACTION_CALL_DURATION";

    public static final String TELEPHONY_EVENT = "TELEPHONY_EVENT";
    public static final String IS_VIDEO = "IS_VIDEO";
    public static final String IS_INCOMING = "IS_INCOMING";
    public static final String PEER_NAME = "PEER_NAME";
    public static final String DURATION = "DURATION";

    Class mCallActivityClass;
    @Keep
    public void onEvent(int event) {
        Log.i("CMV", TelephonyEvent.toString(event));
        Intent intent = new Intent();
        intent.setAction(ACTION_TELEPHONY_EVENT);
        intent.putExtra(TELEPHONY_EVENT, event);
        mLocalBroadcastManager.sendBroadcast(intent);

        switch (event) {
            case UA_EVENT_REGISTERING:
                break;
            case UA_EVENT_REGISTER_FAIL:
                break;
            case UA_EVENT_REGISTER_OK:
                break;
            case UA_EVENT_CALL_INCOMING:
                Intent incomingCall = new Intent(TelephonyService.this, mCallActivityClass);
                incomingCall.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                incomingCall.putExtra(IS_VIDEO, is_video_call());
                incomingCall.putExtra(IS_INCOMING, true);
                String peerName = get_peer_name();
                if(peerName.contains("sip:")) {
                   peerName = peerName.split(":")[1].split("@")[0]; //sip:text_B@123.31.47.34
                }
                incomingCall.putExtra(PEER_NAME, peerName);
                startActivity(incomingCall);
                break;
            case UA_EVENT_CALL_RINGING:
                incomingCall = new Intent(TelephonyService.this, mCallActivityClass);
                incomingCall.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                incomingCall.putExtra(IS_VIDEO, is_video_call());
                incomingCall.putExtra(IS_INCOMING, false);
                peerName = get_peer_name();
                if(peerName.contains("sip:")) {
                    peerName = peerName.split(":")[1].split("@")[0]; //sip:text_B@123.31.47.34
                }
                incomingCall.putExtra(PEER_NAME, peerName);
                startActivity(incomingCall);
                break;
            case UA_EVENT_CALL_CLOSED:
                isCallActive = false;
                incomingCall = new Intent(ACTION_CALL_CLOSE);
                incomingCall.putExtra(IS_VIDEO, is_video_call());
                mLocalBroadcastManager.sendBroadcast(incomingCall);
                break;
            case UA_EVENT_CALL_ESTABLISHED:
                incomingCall = new Intent(ACTION_CALL_ESTABLISHED);
                incomingCall.putExtra(IS_VIDEO, is_video_call());
                mLocalBroadcastManager.sendBroadcast(incomingCall);
                isCallActive = true;
                if(isCallActive) {
                    mHandler.sendEmptyMessageDelayed(TASK_GET_DURATION, 900);
                }
                break;
        }
    }

    static boolean isCallActive = false;
    private static LocalBroadcastManager mLocalBroadcastManager;

}
