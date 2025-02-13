ANDROID_SDK = $(HOME)/Library/Android/sdk
ANDROID_JAR = $(ANDROID_SDK)/platforms/android-35/android.jar
CC          = $(ANDROID_SDK)/ndk/27.2.12479018/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android35-clang
CFLAGS      = -I../.deps/include
LDFLAGS     = -shared -fPIC -lGLESv3 -legl -llog -landroid -lm

.PHONE: all clean

all: app.apk

lib/arm64-v8a/libapp.so: main.c
	@mkdir -p lib/arm64-v8a
	$(CC) $(CFLAGS) main.c libffmpeg.a -o lib/arm64-v8a/libapp.so $(LDFLAGS)

app.unsigned.apk: lib/arm64-v8a/libapp.so
	aapt package -f -M AndroidManifest.xml -I $(ANDROID_JAR) -F app.unsigned.apk
	aapt add app.unsigned.apk lib/arm64-v8a/libapp.so > /dev/null

app.apk: app.unsigned.apk
	apksigner sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out app.apk app.unsigned.apk

install: app.apk
	adb install app.apk > /dev/null

launch: install
	adb shell am start -n "com.livsho.pl/android.app.NativeActivity" > /dev/null

clean:
	rm -rf **.apk **.unsigned.apk build/
