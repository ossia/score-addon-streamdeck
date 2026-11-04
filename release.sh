#!/bin/bash
rm -rf release
mkdir -p release

cp -rf StreamDeck *.{hpp,cpp,txt,json,md} LICENSE release/

mv release score-addon-streamdeck
7z a score-addon-streamdeck.zip score-addon-streamdeck
