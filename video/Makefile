SDK     	= $(HOME)/Library/Android/sdk
JAR = $(SDK)/platforms/android-21/android.jar
CC      	= $(SDK)/ndk/27.2.12479018/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android35-clang++
CFLAGS  	= -std=c++11 -I/usr/local/ffmpeg/include
LDFLAGS 	= -shared -fPIC -lGLESv3 -legl -llog -lc -lm -landroid -lc++_static -nodefaultlibs

.PHONE: all clean

all: app.apk

lib/arm64-v8a/libvideo.so: main.cpp
	@mkdir -p lib/arm64-v8a
	$(CC) $(CFLAGS) libffmpeg_android.a main.cpp -o lib/arm64-v8a/libvideo.so $(LDFLAGS)

app.unsigned.apk: lib/arm64-v8a/libvideo.so
	aapt package -f -M AndroidManifest.xml -I $(JAR) -F app.unsigned.apk
	aapt add app.unsigned.apk lib/arm64-v8a/libvideo.so > /dev/null

app.apk: app.unsigned.apk
	apksigner sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out app.apk app.unsigned.apk

install: app.apk
	adb install app.apk > /dev/null

launch: install
	adb shell am start -n "com.livsho.pl/android.app.NativeActivity" > /dev/null

clean:
	rm -rf **.apk **.unsigned.apk build/
