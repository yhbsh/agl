ANDROID_SDK = $(HOME)/Library/Android/sdk
ANDROID_JAR = $(ANDROID_SDK)/platforms/android-35/android.jar
CC          = $(HOME)/Library/Android/sdk/ndk/27.2.12479018/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android35-clang

CFLAGS  = -O3 -Wall
LDFLAGS = -shared -fPIC -llog -landroid -lcamera2ndk -lmediandk -lc -lm

.PHONE: all clean

all: main.apk

lib/arm64-v8a/libmain.so: main.c
	mkdir -p lib/arm64-v8a
	$(CC) $(CFLAGS) main.c -o lib/arm64-v8a/libmain.so $(LDFLAGS)

main.unsigned.apk: lib/arm64-v8a/libmain.so
	aapt package -f -M AndroidManifest.xml -I $(ANDROID_JAR) -F main.unsigned.apk
	aapt add main.unsigned.apk lib/arm64-v8a/libmain.so >& /dev/null

main.apk: main.unsigned.apk
	apksigner sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out main.apk main.unsigned.apk

install: main.apk
	adb install main.apk >& /dev/null

launch: install
	adb shell am start -n "com.example.camera/android.app.NativeActivity" >& /dev/null

clean:
	rm -rf **.apk **.unsigned.apk build/
