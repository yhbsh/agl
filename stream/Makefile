ANDROID_SDK = $(HOME)/Library/Android/sdk
ANDROID_JAR = $(ANDROID_SDK)/platforms/android-34/android.jar
CC          = $(ANDROID_SDK)/ndk/27.1.12297006/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android34-clang
STRIP       = $(ANDROID_SDK)/ndk/27.1.12297006/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip

CFLAGS  = -O3 -Wall -Wextra -I../../.deps/include
LDFLAGS = -L../../.deps/lib -shared -fPIC -llog -landroid -lcamera2ndk -lmediandk -lc -lm -lavformat -lavcodec -lavdevice -lavutil -lx264

.PHONE: all clean

all: package

package:
	@mkdir -p lib/arm64-v8a
	$(CC) $(CFLAGS) stream.c -o lib/arm64-v8a/libstream.so $(LDFLAGS)
	@$(STRIP) lib/arm64-v8a/libstream.so
	aapt package -f -M AndroidManifest.xml -I $(ANDROID_JAR) -F stream.unsigned.apk
	aapt add stream.unsigned.apk lib/arm64-v8a/libstream.so > /dev/null
	apksigner sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out stream.apk stream.unsigned.apk
	@rm -rf **.idsig **.unsigned.apk lib

install: package
	@adb install stream.apk > /dev/null

launch: install
	@adb shell am start -n "com.example.stream/android.app.NativeActivity" > /dev/null

clean:
	rm -rf **.apk **.unsigned.apk build/
