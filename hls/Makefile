SDK = $(HOME)/Library/Android/sdk
JAR = $(SDK)/platforms/android-34/android.jar

.PHONY: all clean install launch

all: launch

classes.dex: App.java
	javac -g:none -O -cp $(JAR) *.java
	d8 --lib $(JAR) --release *.class --output .
	rm -f *.class

hls.apk: AndroidManifest.xml classes.dex
	aapt package -f -M AndroidManifest.xml -I $(JAR) -F hls.unsigned.apk
	aapt add hls.unsigned.apk classes.dex > /dev/null
	apksigner sign --ks ~/.gradle/debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android --out hls.apk hls.unsigned.apk

install: hls.apk
	@adb install -r hls.apk > /dev/null

launch: install
	@adb shell am start -n "com.example.hls/.App" > /dev/null

clean:
	rm -f *.apk *.idsig *.dex
	rm -rf lib
