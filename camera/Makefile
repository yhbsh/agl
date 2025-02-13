ANDROID_SDK = $(HOME)/Library/Android/sdk
ANDROID_JAR = $(ANDROID_SDK)/platforms/android-34/android.jar
CC          = $(HOME)/Library/Android/sdk/ndk/27.2.12479018/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android34-clang

CFLAGS  = -O3 -Wall -Wextra
LDFLAGS = -shared -fPIC -llog -landroid -lcamera2ndk -lmediandk -lc -lm

.PHONE: all clean

all: package

package:
	@mkdir -p lib/arm64-v8a
	$(CC) $(CFLAGS) camera.c -o lib/arm64-v8a/libcamera.so $(LDFLAGS)
	aapt package -f -M AndroidManifest.xml -I $(ANDROID_JAR) -F camera.unsigned.apk
	aapt add camera.unsigned.apk lib/arm64-v8a/libcamera.so > /dev/null
	apksigner sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out camera.apk camera.unsigned.apk
	@rm -rf **.idsig **.unsigned.apk lib

install: package
	@adb install camera.apk > /dev/null

launch: install
	@adb shell am start -n "com.example.camera/android.app.NativeActivity" > /dev/null

clean:
	rm -rf **.apk **.unsigned.apk build/
