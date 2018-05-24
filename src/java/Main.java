import com.dusun.ubus.UBus;

public class Main {
		 public void on_message(String msg) {
			System.out.println("[RECV]:" + msg);
		 }
     public static void main(String[] args) {
        UBus ubus = new UBus();

        System.out.println(ubus.get_version());

				int h = ubus.connect("DS.GATEWAY", "DS.CLOUD");

				//ubus.send(h, "{\"value\":\"111\"}");
				while ( true ) {
					/*
					try {
						Thread.sleep(1000);
					} catch (InterruptException e) {
						e.printStackTrace();
					}
					*/
					try {
						String s = ubus.recv(h, 4, 80);
						System.out.println("[" +  s  + "]");
					} catch(Exception e) {
						continue;
					}
				}
    }
}
