SDK       = $(HOME)/Library/Android/sdk
ADB       = $(SDK)/platform-tools/adb
CC        = $(SDK)/ndk/21.1.6352462/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang
CXXFLAGS  = -std=c99 -Wall -shared -fPIC 
LDFLAGS   = -lGLESv3 -legl -llog -lc -lm -landroid -lc++_static -nodefaultlibs

.PHONY: all clean

all: grid.apk

lib/arm64-v8a/libgrid.so: main.c
	@mkdir -p lib/arm64-v8a
	$(CC) $(CXXFLAGS) main.c -o lib/arm64-v8a/libgrid.so $(LDFLAGS)

grid.unsigned.apk: lib/arm64-v8a/libgrid.so
	aapt package -f -M AndroidManifest.xml -I $(SDK)/platforms/android-21/android.jar -F grid.unsigned.apk
	aapt add grid.unsigned.apk lib/arm64-v8a/libgrid.so > /dev/null

grid.apk: grid.unsigned.apk
	apksigner sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out grid.apk grid.unsigned.apk

install: grid.apk
	@adb install grid.apk > /dev/null

launch: install
	@adb shell am start -n "com.example.grid/android.app.NativeActivity" > /dev/null

clean:
	rm -rf *.apk *.unsigned.apk
