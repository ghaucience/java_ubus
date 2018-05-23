package com.dusun.ubus;
public class UBus {
	static {
		//System.loadLibrary("ubox");
		//System.loadLibrary("json-c");
		//System.loadLibrary("blobmsg_json");
		//System.loadLibrary("ubus");
		System.loadLibrary("dusun_ubus");
	}
	public native static String get_version();
	public native static int connect(String sListenPat, String sDestPat);
	public native static int close(); 
	public native static int send(String msg);
	public native static int recv(int sec, int usec);
	public void on_message(String msg) {
		System.out.println("[RECV]:" + msg);
	}
}
