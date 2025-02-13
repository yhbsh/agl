SDK     = $(HOME)/Library/Android/sdk
ADB     = $(SDK)/platform-tools/adb
AAPT    = $(SDK)/build-tools/34.0.0/aapt
SIGNER  = $(SDK)/build-tools/34.0.0/apksigner
NDK     = $(SDK)/ndk/21.1.6352462/toolchains/llvm/prebuilt/darwin-x86_64
CC      = $(NDK)/bin/aarch64-linux-android21-clang
CFLAGS  = -I../../.deps/include -Wall
LDFLAGS = -shared -fPIC -L$(NDK)/sysroot/usr/lib/aarch64-linux-android/26 -laaudio -lGLESv3 -legl -llog -landroid -lm

.PHONE: all clean

all:
	@mkdir -p lib/arm64-v8a
	$(CC) $(CFLAGS) audio.c -o lib/arm64-v8a/libaudio.so $(LDFLAGS)
	$(AAPT) package -f -M AndroidManifest.xml -I $(SDK)/platforms/android-21/android.jar -F audio.unsigned.apk
	$(AAPT) add audio.unsigned.apk lib/arm64-v8a/libaudio.so > /dev/null
	$(AAPT) add audio.unsigned.apk assets/file.wav > /dev/null
	$(SIGNER) sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out audio.apk audio.unsigned.apk
	@rm -rf audio.unsigned.apk audio.apk.idsig

install: all
	@$(ADB) install audio.apk > /dev/null

launch: install
	@$(ADB) shell am start -n "com.example.audio/android.app.NativeActivity" > /dev/null

clean:
	rm -rf *.apk *.unsigned.apk
