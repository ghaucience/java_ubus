###################################################################
# Test Run Library
# ################
#export LD_LIBRARY_PATH=$(shell pwd)/lib


###################################################################
# Cross Setting 
# #############
CROSS   ?= 
#CROSS   ?= arm-cortexa9-linux-gnueabihf-

ifeq ($(CROSS),arm-cortexa9-linux-gnueabihf-)
CROSSTOOLDIR 				:=/home/au/all/gwork/tmp/ember_for_arm
export  STAGING_DIR := $(CROSSTOOLDIR)/4.9.3/staging_dir
export  PATH 				:=$(PATH):$(CROSSTOOLDIR)/4.9.3/bin
CROSS_CFLAGS				:= -I$(CROSSTOOLDIR)/root-arm/usr/local/include
CROSS_LDFLAGHS			:= -L$(CROSSTOOLDIR)/root-arm/usr/local/lib
endif


###################################################################
# Java Home Env
# #############
JAVA_INC=/home/au/all/opt/jdk1.8.0_144/include
#JAVA_INC=/usr/java/jdk1.7.0_71/include
#JAVA_INC=/home/au/all/dl/jdk1.8.0_161/include
CFLAGS=-fPIC -I./inc -I./src/c/ -I$(JAVA_INC) -I$(JAVA_INC)/linux
LDFLAGS=-shared -fPIC -L/usr/lib/ -lubus -lubox -lblobmsg_json -ljson-c


###################################################################
# Rules
# #####
all:
	mkdir -p ./build
	mkdir -p ./build/src/c/
	mkdir -p ./build/src/java/
	mkdir -p ./lib
	javac -d ./build ./src/java/UBus.java
	javah -classpath ./build -d ./inc -jni com.dusun.ubus.UBus
	javac -classpath ./build -d ./build ./src/java/Main.java

	$(CROSS)gcc -c ./src/c/com_dusun_ubus_UBus.c -D_REENTRANT $(CFLAGS) $(CROSS_CFLAGS) -o ./build/src/c/com_dusun_ubus_UBus.o
	$(CROSS)gcc -c ./src/c/list.c -D_REENTRANT $(CFLAGS) $(CROSS_CFLAGS) -o ./build/src/c/list.o
	$(CROSS)gcc  ./build/src/c/com_dusun_ubus_UBus.o ./build/src/c/list.o $(CROSS_LDFLAGHS) $(LDFLAGS) -o ./lib/libdusun_ubus.so
	
	echo 123456 | sudo -S -u root cp ./lib/libdusun_ubus.so /usr/lib/
run:
	java -classpath ./build -Djava.library.path=/usr/lib/ Main
	#echo 123456 | sudo -S -u root java -classpath ./build  Main

clean:
	rm -rf ./build
	rm -rf ./inc
	rm -rf ./lib
