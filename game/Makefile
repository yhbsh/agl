ANDROID_SDK = $(HOME)/Library/Android/sdk
ANDROID_JAR = $(ANDROID_SDK)/platforms/android-21/android.jar
CC          = $(HOME)/Library/Android/sdk/ndk/21.1.6352462/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang

CFLAGS  = -Os -Wall -Wextra -I../../.deps/include
LDFLAGS = -shared -fPIC -lGLESv3 -legl -llog -landroid -lm

.PHONE: all clean

all: package

package:
	@mkdir -p lib/arm64-v8a
	$(CC) $(CFLAGS) game.c -o lib/arm64-v8a/libgame.so $(LDFLAGS)
	aapt package -f -M AndroidManifest.xml -I $(ANDROID_JAR) -F game.unsigned.apk
	aapt add game.unsigned.apk lib/arm64-v8a/libgame.so > /dev/null
	aapt add game.unsigned.apk assets/shaders.glsl > /dev/null
	aapt add game.unsigned.apk assets/image.jpeg > /dev/null
	apksigner sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out game.apk game.unsigned.apk
	@rm -rf **.idsig **.unsigned.apk lib

install: package
	@adb install game.apk > /dev/null

launch: install
	@adb shell am start -n "com.example.game/android.app.NativeActivity" > /dev/null

clean:
	rm -rf **.apk **.unsigned.apk build/
