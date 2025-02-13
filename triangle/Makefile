SDK     = $(HOME)/Library/Android/sdk
ADB     = $(SDK)/platform-tools/adb
AAPT    = $(SDK)/build-tools/34.0.0/aapt
SIGNER  = $(SDK)/build-tools/34.0.0/apksigner
CC      = $(SDK)/ndk/21.1.6352462/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang
CFLAGS  = -Wall -Wextra
LDFLAGS = -shared -fPIC -lGLESv3 -legl -llog -landroid -lm

.PHONE: all clean

all:
	@mkdir -p lib/arm64-v8a
	$(CC) $(CFLAGS) triangle.c -o lib/arm64-v8a/libtriangle.so $(LDFLAGS)
	$(AAPT) package -f -M AndroidManifest.xml -I $(SDK)/platforms/android-21/android.jar -F triangle.unsigned.apk
	$(AAPT) add triangle.unsigned.apk lib/arm64-v8a/libtriangle.so > /dev/null
	$(SIGNER) sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out triangle.apk triangle.unsigned.apk
	@rm -rf triangle.unsigned.apk triangle.apk.idsig

install: all
	@$(ADB) install triangle.apk > /dev/null

launch: install
	@$(ADB) shell am start -n "com.example.triangle/android.app.NativeActivity" > /dev/null

clean:
	rm -rf *.apk *.unsigned.apk
