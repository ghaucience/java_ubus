#include <jni.h>

#include <stdio.h>
#include <libubox/blobmsg_json.h>
#include <libubox/avl.h>
#include <libubus.h>
#include <syslog.h>
#include <jansson.h>

#include "list.h"


#if 0
#define log_debug(...)	do { syslog(LOG_CONS | LOG_DEBUG, __VA_ARGS__); printf(__VA_ARGS__); } while (0)
#define log_warn(...)		do { syslog(LOG_CONS | LOG_WARNING,  __VA_ARGS__); printf(__VA_ARGS__); } while (0)
#else
#define log_debug(...)	do {  } while (0)
#define log_warn(...)		do {  } while (0)
#endif

#define _offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define _container_of(ptr, type, member) ({                      \
                      const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
                       (type *)( (char *)__mptr - offsetof(type,member) );})

typedef struct stDusunUBux {
	struct ubus_context *ubus_ctx;
	struct ubus_event_handler ubus_listener;
	char listen_pat[64];
	char dest_pat[64];
	JavaVM *jvm;
	jobject jobj;
	struct blob_buf b;
	char *msg;
}stDusunUBux_t;

/*
static struct ubus_context *dusun_ubus_ctx = NULL;
static struct ubus_event_handler dusun_ubus_listener;
static struct blob_buf dusun_b;
*/

static void dusun_ubus_receive_event(struct ubus_context *ctx,
																		 struct ubus_event_handler *ev, 
																		 const char *type,
																		 struct blob_attr *msg);

static void dusun_ubus_connect(stDusunUBux_t *db);
static void dusun_ubus_disconnect(stDusunUBux_t *db);
static void dusun_ubus_send(stDusunUBux_t *db, char *str);
static jstring dusun_ubus_receive_timeout(JNIEnv *je, stDusunUBux_t *db, int sec, int usec);

static char gListenPat[256];
static char gDestPat[256];
static JavaVM *gJavaVM = NULL;
static jobject gJavaObj;

static stList_t list = {0};

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
	stDusunUBux_t *db = (stDusunUBux_t *)malloc(sizeof(stDusunUBux_t));
	if (db == NULL) {
		return 0;
	}
	(*je)->GetJavaVM(je, &db->jvm);
	db->jobj = (*je)->NewGlobalRef(je, jc);
	strcpy(db->listen_pat, (*je)->GetStringUTFChars(je, listenPat, NULL));
	strcpy(db->dest_pat, (*je)->GetStringUTFChars(je, destPat, NULL));

	dusun_ubus_connect(db);
	db->msg = NULL;
	list_push_front(&list, db);	

	return (jint)db;
}

/*
 * Class:     com_dusun_ubus_UBus
 * Method:    close
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_dusun_ubus_UBus_close(JNIEnv *je, jclass jc, jint handler) {
	stDusunUBux_t *db = (stDusunUBux_t *)handler;
	if (db == NULL || !list_search(&list, db)) {
		return 0;
	}

	list_remove(&list, db);
	dusun_ubus_disconnect(db);
	free(db);
	return 0;
}

/*
 * Class:     com_dusun_ubus_UBus
 * Method:    send
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_dusun_ubus_UBus_send(JNIEnv *je, jclass jc, jint handler, jstring msg) {
	stDusunUBux_t *db = (stDusunUBux_t *)handler;
	if (db == NULL || !list_search(&list, db)) {
		return 0;
	}

	char *smsg = NULL;
	smsg = (char *)(*je)->GetStringUTFChars(je, msg, NULL);

	dusun_ubus_send(db->ubus_ctx, smsg);

	return 0;
}

/*
 * Class:     com_dusun_ubus_UBus
 * Method:    recv
 * Signature: (II)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_dusun_ubus_UBus_recv(JNIEnv *je, jclass jc, jint handler, jint sec, jint usec) {
	stDusunUBux_t *db = (stDusunUBux_t *)handler;
	if (db == NULL || !list_search(&list, db)) {
		return 0;
	}
	return dusun_ubus_receive_timeout(je, db, sec, usec);

}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void dusun_ubus_connect(stDusunUBux_t *db) {
	db->ubus_ctx = ubus_connect(NULL);
	memset(&db->ubus_listener, 0, sizeof(db->ubus_listener));
	db->ubus_listener.cb = dusun_ubus_receive_event;
	ubus_register_event_handler(db->ubus_ctx, &db->ubus_listener, db->listen_pat);
}
static void dusun_ubus_disconnect(stDusunUBux_t *db) {
	ubus_unregister_event_handler(db->ubus_ctx, &db->ubus_listener);
	ubus_free(db->ubus_ctx);
}
static void dusun_ubus_send(stDusunUBux_t *db, char *str) {
	blob_buf_init(&db->b, 0);
	blobmsg_add_string(&db->b, "PKT", str);

	log_debug("[SEND]: <%s>\n", str);

	ubus_send_event(db->ubus_ctx, db->dest_pat, db->b.head);
}


static cmp_db_by_ctx_ptr(void *data, void *ctx) {
	stDusunUBux_t *db = (stDusunUBux_t*)data;
	if ((void *)db->ubus_ctx == ctx) {
		return 0;
	}
	return 1;
}
static void dusun_ubus_receive_event(struct ubus_context *ctx,struct ubus_event_handler *ev, const char *type,struct blob_attr *msg) {
	char *str;
	log_debug("-----------------[ubus msg]: handler ....-----------------\n");
	str = blobmsg_format_json(msg, true);
	if (str != NULL) {
		log_debug("[ubus msg]: [%s]\n", str);

		//uproto_handler_cmd_alink(spkt);
		//protos[ue.protoused].handler(spkt);

		//stDusunUBux_t *db = (stDusunUBux_t*)_container_of(ctx, stDusunUBux_t, ubus_ctx);
		stDusunUBux_t *db = (stDusunUBux_t*)list_search_compare(&list, cmp_db_by_ctx_ptr, ctx);
		if (db == NULL) {
			db->msg = NULL;
			return;
		}

		db->msg = strdup(str);
		log_debug("db->msg:%p\n", db->msg);

		free(str);
	} 
	log_debug("-----------------[ubus msg]: handler over-----------------\n");
}

static jstring dusun_ubus_receive_timeout(JNIEnv *je, stDusunUBux_t *db, int sec, int usec) {
  struct timeval tv;
  int maxfd;
  fd_set fds;

  tv.tv_sec = sec;
  tv.tv_usec  = usec;

  FD_ZERO(&fds);
  FD_SET(db->ubus_ctx->sock.fd, &fds);

  maxfd = db->ubus_ctx->sock.fd;

  int ret = select(maxfd + 1, &fds, NULL, NULL, &tv);

  if (ret <= 0) {
		return (*je)->NewStringUTF(je, "");
  }

  if (!FD_ISSET(db->ubus_ctx->sock.fd, &fds)) {
		return (*je)->NewStringUTF(je, "");
  }

	if (db->msg != NULL) {
		free(db->msg);
		db->msg = NULL;
	}
	log_debug("----------------1 \n");
	ubus_handle_event(db->ubus_ctx);
	log_debug("----------------2 \n");
		log_debug("db->msg:%p\n", db->msg);
	if (db->msg == NULL) {
		return (*je)->NewStringUTF(je, "");
	} else {
		return (*je)->NewStringUTF(je, db->msg);
	}
}
