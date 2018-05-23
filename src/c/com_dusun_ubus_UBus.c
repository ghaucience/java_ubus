#include <jni.h>

#include <stdio.h>
#include <libubox/blobmsg_json.h>
#include <libubox/avl.h>
#include <libubus.h>
#include <syslog.h>
#include <jansson.h>


#define log_debug(...)	do { syslog(LOG_CONS | LOG_DEBUG, __VA_ARGS__); printf(__VA_ARGS__); } while (0)
#define log_warn(...)		do { syslog(LOG_CONS | LOG_WARNING,  __VA_ARGS__); printf(__VA_ARGS__); } while (0)

static struct ubus_context *dusun_ubus_ctx = NULL;
static struct ubus_event_handler dusun_ubus_listener;
static void dusun_ubus_init();
static void dusun_ubus_send(char *str);
static struct blob_buf dusun_b;
static void dusun_ubus_receive_event(struct ubus_context *ctx,struct ubus_event_handler *ev, const char *type,struct blob_attr *msg);
static int dusun_ubus_receive_timeout(int sec, int usec);

static char gListenPat[256];
static char gDestPat[256];

void *hLibUBus = NULL;
void *hLibUBox = NULL;
void *hLibBlob = NULL;
static JavaVM *gJavaVM = NULL;
static jobject gJavaObj;


/*
 * Class:     com_dusun_ubus_UBus
 * Method:    get_version
 * Signature: ()I
 */
JNIEXPORT jstring JNICALL Java_com_dusun_ubus_UBus_get_1version(JNIEnv *je, jclass jc) {
	return (*je)->NewStringUTF(je, "libdusun_ubus_1.0.0");
}

/*
 * Class:     com_dusun_ubus_UBus
 * Method:    connect
 * Signature: (Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_dusun_ubus_UBus_connect(JNIEnv *je, jclass jc, jstring listenPat, jstring destPat) {
	if (gJavaVM == NULL) {
		(*je)->GetJavaVM(je, &gJavaVM);
		gJavaObj = (*je)->NewGlobalRef(je, jc);


		strcpy(gListenPat, (*je)->GetStringUTFChars(je, listenPat, NULL));
		strcpy(gDestPat, (*je)->GetStringUTFChars(je, destPat, NULL));

		dusun_ubus_init();
	}
	return 0;
}

/*
 * Class:     com_dusun_ubus_UBus
 * Method:    close
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_dusun_ubus_UBus_close(JNIEnv *je, jclass jc) {
	/* TODO */
	return 0;
}

/*
 * Class:     com_dusun_ubus_UBus
 * Method:    send
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_dusun_ubus_UBus_send(JNIEnv *je, jclass jc, jstring msg) {

	char *smsg = NULL;
	smsg = (char *)(*je)->GetStringUTFChars(je, msg, NULL);
	dusun_ubus_send(smsg);

	return 0;
}

/*
 * Class:     com_dusun_ubus_UBus
 * Method:    recv
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL Java_com_dusun_ubus_UBus_recv(JNIEnv *je, jclass jc, jint sec, jint usec) {
	dusun_ubus_receive_timeout(sec, usec);
	return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void dusun_ubus_init() {
	dusun_ubus_ctx = ubus_connect(NULL);
	memset(&dusun_ubus_listener, 0, sizeof(dusun_ubus_listener));
	dusun_ubus_listener.cb = dusun_ubus_receive_event;
	ubus_register_event_handler(dusun_ubus_ctx, &dusun_ubus_listener, gListenPat);
}
static void dusun_ubus_send(char *str) {
	blob_buf_init(&dusun_b, 0);
	blobmsg_add_string(&dusun_b, "PKT", str);
	log_debug("Send: str:%s\n", str);

	ubus_send_event(dusun_ubus_ctx, gDestPat, dusun_b.head);
}


static void dusun_ubus_receive_event(struct ubus_context *ctx,struct ubus_event_handler *ev, const char *type,struct blob_attr *msg) {
	char *str;
	log_debug("-----------------[ubus msg]: handler ....-----------------\n");
	str = blobmsg_format_json(msg, true);
	if (str != NULL) {
		log_debug("[ubus msg]: [%s]\n", str);

		//uproto_handler_cmd_alink(spkt);
		//protos[ue.protoused].handler(spkt);

#if 1
		JNIEnv *env = NULL;
		(*gJavaVM)->AttachCurrentThread(gJavaVM, (void **)&env, NULL);

		jclass javaClass = (*env)->GetObjectClass(env, gJavaObj);
		if (javaClass == NULL) {
			log_debug("Failed to Find JavaClass!\n");	
			free(str);
			return;
		}

		jmethodID  javaCallback = (*env)->GetMethodID(env, javaClass, "on_message", "(S)V");
		if (javaCallback == NULL) {
			log_debug("Failed to Find Method on_message!\n");
			free(str);
			return;
		}

		(*env)->CallVoidMethod(env, gJavaObj, javaCallback, (*env)->NewStringUTF(env, str));
#endif

		free(str);
	} 
	log_debug("-----------------[ubus msg]: handler over-----------------\n");
}

static int dusun_ubus_receive_timeout(int sec, int usec) {
  struct timeval tv;
  int maxfd;
  fd_set fds;

  tv.tv_sec = sec;
  tv.tv_usec  = usec;

  FD_ZERO(&fds);
  FD_SET(dusun_ubus_ctx->sock.fd, &fds);

  maxfd = dusun_ubus_ctx->sock.fd;

  int ret = select(maxfd + 1, &fds, NULL, NULL, &tv);

  if (ret <= 0) {
    return 0; /*nothing to do */
  }

  if (!FD_ISSET(dusun_ubus_ctx->sock.fd, &fds)) {
    return 0; /* what happend , maybe ubus stop and restart */
  }

	ubus_handle_event(dusun_ubus_ctx);
}
