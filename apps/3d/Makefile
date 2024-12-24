SDK     = $(HOME)/Library/Android/sdk
ADB     = $(SDK)/platform-tools/adb
AAPT    = $(SDK)/build-tools/34.0.0/aapt
SIGNER  = $(SDK)/build-tools/34.0.0/apksigner
CXX     = $(SDK)/ndk/21.1.6352462/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang++
CFLAGS  = -std=c++17 -Wall -Wextra -fno-exceptions -fno-rtti
LDFLAGS = -shared -fPIC -lGLESv3 -legl -llog -lc -lm -landroid -lc++_static -nodefaultlibs

.PHONY: all clean

all:
	@mkdir -p lib/arm64-v8a
	$(CXX) $(CFLAGS) 3d.cpp -o lib/arm64-v8a/lib3d.so $(LDFLAGS)
	$(AAPT) package -f -M AndroidManifest.xml -I $(SDK)/platforms/android-21/android.jar -F 3d.unsigned.apk
	$(AAPT) add 3d.unsigned.apk lib/arm64-v8a/lib3d.so > /dev/null
	$(SIGNER) sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out 3d.apk 3d.unsigned.apk
	@rm -rf 3d.unsigned.apk 3d.apk.idsig lib

install: all
	@$(ADB) install 3d.apk > /dev/null

launch: install
	@$(ADB) shell am start -n "com.example.tiny3D/android.app.NativeActivity" > /dev/null

clean:
	rm -rf *.apk *.unsigned.apk
