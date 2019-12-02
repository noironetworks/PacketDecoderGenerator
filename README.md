# PacketDecoderGenerator
Generate Stubs for Packet Parsers used for packet logging.

Takes as input a layer definition file(layer.defs) and a license file
(license.in) located in the same directory. Syntax for the layer definition 
is mentioned at the top of the same file. Build with just make and run as ./packetDecoderGenerator

Generates 3 files: PacketDecoderLayers.cpp, PacketDecoderLayers.h and 
PacketDecoder.part. PacketDecoder.part is meant to be copied to the opflexagent::PacketDecoder::configure method in the file PacketDecoder.cpp in opflexagent repository path opflexagent/agent-ovs/ovs. 

