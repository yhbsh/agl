SDK     = $(HOME)/Library/Android/sdk
JAR     = $(SDK)/platforms/android-21/android.jar
CC      = $(SDK)/ndk/21.1.6352462/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang
STRIP   = $(SDK)/ndk/21.1.6352462/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip
CFLAGS  = -I../../.deps/include -Wall
LDFLAGS = -shared -fPIC -L../../.deps/lib -legl -lGLESv3 -llog -lm -landroid -lavformat -lavcodec -lavutil -lswscale
 
.PHONY: all clean install launch

all: activity.apk

classes.dex: MainActivity.java
	javac -cp $(JAR) MainActivity.java
	d8 --lib $(JAR) *.class --output .
	rm -f *.class

lib/arm64-v8a/libengine.so: engine.c
	@mkdir -p lib/arm64-v8a
	$(CC) $(CFLAGS) engine.c -o lib/arm64-v8a/libengine.so $(LDFLAGS)
	$(STRIP) lib/arm64-v8a/libengine.so

activity.apk: AndroidManifest.xml classes.dex lib/arm64-v8a/libengine.so
	aapt package -f -M AndroidManifest.xml -I $(JAR) -F activity.unsigned.apk
	aapt add activity.unsigned.apk classes.dex > /dev/null
	aapt add activity.unsigned.apk lib/arm64-v8a/libengine.so > /dev/null
	apksigner sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out activity.apk activity.unsigned.apk
	@rm -rf activity.apk.idsig activity.unsigned.apk classes.dex lib

install: activity.apk
	@adb install -r activity.apk > /dev/null

launch: install
	@adb shell am start -n "com.example.activity/.MainActivity" > /dev/null

clean:
	rm -f *.apk *.idsig
	rm -rf lib
