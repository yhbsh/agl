SDK       = $(HOME)/Library/Android/sdk
ADB       = $(SDK)/platform-tools/adb
CXX       = $(SDK)/ndk/21.1.6352462/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang++
CXXFLAGS  = -std=c++11 -Wall -shared -fPIC 
LDFLAGS   = -lGLESv3 -legl -llog -lc -lm -landroid -lc++_static -nodefaultlibs

.PHONY: all clean

all: package

package:
	@mkdir -p lib/arm64-v8a
	$(CXX) $(CXXFLAGS) grid.cpp -o lib/arm64-v8a/libgrid.so $(LDFLAGS)
	aapt package -f -M AndroidManifest.xml -I $(SDK)/platforms/android-21/android.jar -F grid.unsigned.apk
	aapt add grid.unsigned.apk lib/arm64-v8a/libgrid.so > /dev/null
	apksigner sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out grid.apk grid.unsigned.apk
	@rm -rf grid.unsigned.apk grid.apk.idsig lib

install: package
	@adb install grid.apk > /dev/null

launch: install
	@adb shell am start -n "com.example.grid/android.app.NativeActivity" > /dev/null

clean:
	rm -rf *.apk *.unsigned.apk
