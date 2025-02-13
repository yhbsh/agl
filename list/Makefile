SDK       = $(HOME)/Library/Android/sdk
ADB       = $(SDK)/platform-tools/adb
CXX       = $(SDK)/ndk/21.1.6352462/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang
CXXFLAGS  = -std=c++11 -Wall -shared -fPIC 
LDFLAGS   = -lGLESv3 -legl -llog -lc -lm -landroid -lc++_static -nodefaultlibs

.PHONE: all clean

all:
	@mkdir -p lib/arm64-v8a
	$(CXX) $(CXXFLAGS) list.cpp -o lib/arm64-v8a/liblist.so $(LDFLAGS)
	aapt package -f -M AndroidManifest.xml -I $(SDK)/platforms/android-21/android.jar -F list.unsigned.apk
	aapt add list.unsigned.apk lib/arm64-v8a/liblist.so > /dev/null
	apksigner sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out list.apk list.unsigned.apk
	@rm -rf list.unsigned.apk list.apk.idsig lib

install: all
	@$(ADB) install list.apk > /dev/null

launch: install
	@$(ADB) shell am start -n "com.example.list/android.app.NativeActivity" > /dev/null

clean:
	rm -rf *.apk *.unsigned.apk
