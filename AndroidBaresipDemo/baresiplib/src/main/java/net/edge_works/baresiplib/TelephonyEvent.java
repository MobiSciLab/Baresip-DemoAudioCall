package net.edge_works.baresiplib;

public class TelephonyEvent {
    public static final int UA_EVENT_REGISTERING = 0;   //0
    public static final int UA_EVENT_REGISTER_OK = 1;
    public static final int UA_EVENT_REGISTER_FAIL = 2;
    public static final int UA_EVENT_UNREGISTERING = 3;
    public static final int UA_EVENT_SHUTDOWN = 4;
    public static final int UA_EVENT_EXIT = 5;

    public static final int UA_EVENT_CALL_INCOMING = 6;
    public static final int UA_EVENT_CALL_RINGING = 7;
    public static final int UA_EVENT_CALL_PROGRESS = 8;
    public static final int UA_EVENT_CALL_ESTABLISHED = 9;
    public static final int UA_EVENT_CALL_CLOSED = 10;
    public static final int UA_EVENT_CALL_TRANSFER_FAILED = 11;
    public static final int UA_EVENT_CALL_DTMF_START = 12;
    public static final int UA_EVENT_CALL_DTMF_END = 13;

    public static final int UA_EVENT_MAX = 14;

    public static String toString(int event) {
        switch (event) {
            case UA_EVENT_REGISTERING:
                return "UA_EVENT_REGISTERING";

            case UA_EVENT_REGISTER_OK:
                return "UA_EVENT_REGISTER_OK";

            case UA_EVENT_REGISTER_FAIL:
                return "UA_EVENT_REGISTER_FAIL";

            case UA_EVENT_UNREGISTERING:
                return "UA_EVENT_UNREGISTERING";

            case UA_EVENT_CALL_INCOMING:
                return "UA_EVENT_CALL_INCOMING";

            case UA_EVENT_CALL_RINGING:
                return "UA_EVENT_CALL_RINGING";

            case UA_EVENT_CALL_PROGRESS:
                return "UA_EVENT_CALL_PROGRESS";

            case UA_EVENT_CALL_ESTABLISHED:
                return "UA_EVENT_CALL_ESTABLISHED";

            case UA_EVENT_CALL_CLOSED:
                return "UA_EVENT_CALL_CLOSED";

            case UA_EVENT_CALL_TRANSFER_FAILED:
                return "UA_EVENT_CALL_TRANSFER_FAILED";

            case UA_EVENT_CALL_DTMF_START:
                return "UA_EVENT_CALL_DTMF_START";

            case UA_EVENT_CALL_DTMF_END:
                return "UA_EVENT_CALL_DTMF_END";

            case UA_EVENT_MAX:
                return "UA_EVENT_MAX";

            default:
                return "UNKNOWN";
        }
    }
}
