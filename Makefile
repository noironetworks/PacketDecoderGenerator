all:
	g++ --std=c++11 -g generateHeaderParser.cpp -o packetDecoderGenerator
clean:
	rm -f packetDecoderGenerator
