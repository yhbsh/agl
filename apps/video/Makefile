SDK     = $(HOME)/Library/Android/sdk
ADB     = $(SDK)/platform-tools/adb
AAPT    = $(SDK)/build-tools/34.0.0/aapt
SIGNER  = $(SDK)/build-tools/34.0.0/apksigner
CC      = $(SDK)/ndk/21.1.6352462/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang++
CFLAGS  = -std=c++11 -I../../.deps/include -Wall -Wextra
LDFLAGS = -shared -fPIC -L../../.deps/lib -lGLESv3 -legl -llog -lc -lm -landroid -lavformat -lavcodec -lavutil -lc++_static -nodefaultlibs -lgcc

.PHONE: all clean

all:
	@mkdir -p lib/arm64-v8a
	$(CC) $(CFLAGS) video.cpp -o lib/arm64-v8a/libvideo.so $(LDFLAGS)
	$(AAPT) package -f -M AndroidManifest.xml -I $(SDK)/platforms/android-21/android.jar -F video.unsigned.apk
	$(AAPT) add video.unsigned.apk lib/arm64-v8a/libvideo.so > /dev/null
	$(SIGNER) sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out video.apk video.unsigned.apk
	@rm -rf video.unsigned.apk video.apk.idsig lib

install: all
	@$(ADB) install video.apk > /dev/null

launch: install
	@$(ADB) shell am start -n "com.example.video/android.app.NativeActivity" > /dev/null

clean:
	rm -rf *.apk *.unsigned.apk
