/*
 * TODO: Accept comment lines everywhere in the file
 *       Add file line number context to errors
 *       Add map insertion failure errors ( possible due to duplicate names)   
 *
 *
 */
#include <fstream>
#include <iostream>
#include <string>
#include <array>
#include <unordered_map>
#include <vector>
#include <sstream>

using namespace std;
static uint32_t LayerType=0;
static uint32_t LayerId=0;

static vector<string> GenTypeToOpType = {
    "FLDTYPE_NONE",
    "FLDTYPE_BITFIELD",
    "FLDTYPE_BYTES",
    "FLDTYPE_IPv4ADDR",
    "FLDTYPE_IPv6ADDR",
    "FLDTYPE_MAC",
    "FLDTYPE_VARBYTES"
};

static std::string convertFieldType(string &str) {
    uint32_t opType = 0;
    if(str == "VARBYTES") {
        opType=6;
    } else if(str == "MAC") {
        opType=5;
    } else if (str == "IPV6ADDR") {
        opType=4;
    } else if(str == "IPV4ADDR") {
        opType=3;
    } else if(str == "BYTES") {
        opType=2;
    } else if(str == "BITS") {
        opType=1;
    } else {
        opType=0;
    }
    return GenTypeToOpType[opType];
}

class SubRecord {
public:
    SubRecord():bitLength(0),flag(true),nextKey(false),
            length(false),scratchOffset(-1){};
    void reset() {
        bitLength = 0;
        flag = false;
        nextKey = false;
        length = false;
        scratchOffset = -1;
        fldName.clear();
        fldTypeName.clear();
    }
    uint32_t bitLength, offset;
    int scratchOffset;
    std::string fldName, fldTypeName;
    bool flag, nextKey, length;
};

class Record {
public:
    Record():layerTypeId(0),layerId(0),nextLayerTypeId(0),layerKey(0),length(0),optionLayerTypeId(0),optionLayerId(0){};
    void reset() {
        layerType.clear();
        layerName.clear();
        layerTypeId = 0;
        layerId = 0;
        length = 0;
        optionLayerId = 0;
        optionLayerTypeId = 0;
        optionLayerName.clear();
        fields.clear();
    }
    std::string layerType, layerName, nextLayerType, optionLayerName;
    uint32_t layerTypeId, layerId, layerKey, length, nextLayerTypeId, 
    optionLayerTypeId, optionLayerId;
    vector<SubRecord> fields; 
};

static unordered_map<string, uint32_t> layerNameMap;
static unordered_map<string, uint32_t> layerTypeMap;

static int ReadRecord(fstream &_protoData,Record &rec){
    rec.reset();
    array<char, 200 > buf1;
    while(_protoData.getline(&buf1[0], 200)) {
        string str(buf1.data());
        if(str.find ('}') != string::npos){
            return -2;
        } else if(str.find ('{') != string::npos) {
            break;
        }
        cout << "Error: Record missing starting brace { "
             << endl;
        return -1;
    }
    try {
        _protoData.exceptions(ios_base::failbit | ios_base::badbit);
        _protoData >> rec.layerType >> rec.layerKey 
        >> rec.layerName >> rec.length >> rec.nextLayerType >> rec.optionLayerName;
    } catch (ios_base::failure& fail) {
        cout << "Error: Failed to read layer main data "
        << fail.what()  
        << _protoData.tellg()
        << endl;
        return -1;
    };
    auto type_it = layerTypeMap.find(rec.layerType);
    auto name_it = layerNameMap.find(rec.layerName);
    unordered_map<std::string, uint32_t>::iterator nextType_it = layerTypeMap.end();
    if(rec.nextLayerType != "none") { 
        nextType_it = layerTypeMap.find(rec.nextLayerType);
    }
    unordered_map<std::string, uint32_t>::iterator opt_it = layerNameMap.end();
    unordered_map<std::string, uint32_t>::iterator optType_it = layerTypeMap.end();
    if(rec.optionLayerName != "none") {
        opt_it = layerNameMap.find(rec.optionLayerName);
        optType_it = layerTypeMap.find(rec.optionLayerName);
    }
    if(type_it == layerTypeMap.end()) {
        LayerType++;
        auto it = layerTypeMap.insert(make_pair(rec.layerType,LayerType));
        rec.layerTypeId = LayerType;
    } else {
        rec.layerTypeId = type_it->second;
    }
    if(opt_it == layerNameMap.end()) {
        if(rec.optionLayerName != "none") {
            LayerId++;
            auto it = layerNameMap.insert(make_pair(rec.optionLayerName,LayerId));
            rec.optionLayerId = LayerId;
            LayerType++;
            rec.optionLayerTypeId = LayerType;
            layerTypeMap.insert(make_pair(rec.optionLayerName,LayerType));
        }
    } else {
        rec.optionLayerId = opt_it->second;
	rec.optionLayerTypeId = optType_it->second;
    }
    if(nextType_it == layerTypeMap.end()) {
        if(rec.nextLayerType != "none") {
            LayerType++;
            auto it = layerTypeMap.insert(make_pair(rec.nextLayerType,LayerType));
            rec.nextLayerTypeId = LayerType;
        }
    } else {
        rec.nextLayerTypeId = nextType_it->second;
    }
    if(name_it == layerNameMap.end()) {
        LayerId++;
        auto it = layerNameMap.insert(make_pair(rec.layerName,LayerId));
        rec.layerId = LayerId;
    } else {
        rec.layerId = name_it->second;
    }
    _protoData.seekg(1,ios_base::cur);
    uint32_t offset = 0;
    array<char, 200 > buf2;
    try {
        SubRecord srec;
        _protoData.exceptions(ios_base::failbit | ios_base::badbit);
        while(_protoData.getline(&buf2[0], 200)) {
            srec.reset();
            string str(buf2.data());
            if(str.find('}') != string::npos) {
                throw ios_base::failure("Reached the end of the record");
            }
            istringstream stream(str);
            string opt;
            stream >> srec.fldTypeName >> srec.fldName  >> srec.bitLength 
                    >> srec.flag;
            stream >> opt;
            if(opt.find("Key") != string::npos) {
                srec.nextKey = true;
            }
            if(opt.find("Length") != string::npos) {
                srec.length = true;
            }
            if(opt.find("->") != string::npos) {
                unsigned scratchPadBuf = opt[2]-0x30;
                if((scratchPadBuf <=3) && 
                    (scratchPadBuf >=0)) {
                    srec.scratchOffset = scratchPadBuf;
                }   
            }
            srec.fldTypeName = convertFieldType(srec.fldTypeName);
            srec.offset = offset; 
            offset += srec.bitLength;
            rec.fields.push_back(srec);
        }
        
    } catch( ios_base:: failure& fail) {
        string str(buf2.data());
        if(str.find('}') == string::npos) {
            cout << "Error:Failed to read field data " << endl;
            return 0;
        }
        return 0;
    }
    return 0;
}

int main() {
    fstream licenseFile("./license.in",licenseFile.in);
    fstream protoData("./layer.defs", protoData.in);
    fstream layerHeader("./PacketDecoderLayers.h", layerHeader.out);
    fstream layerImpl("./PacketDecoderLayers.cpp", layerImpl.out);
    fstream decoderImpl("./PacketDecoder.part", decoderImpl.out);
    array<char,200> buf;
    Record rec;
    int lc = 0;
    while(licenseFile.getline(&buf[0],200)) {
        layerHeader << string(&buf[0]) << endl;
        lc++;
        if(lc >= 10)
            break;
    }
    while(licenseFile.getline(&buf[0],200)) {
        layerImpl << string(&buf[0]) << endl;
    }

    layerHeader << "#ifndef __PACKETDECODERLAYERS_H__" << endl;
    layerHeader << "#define __PACKETDECODERLAYERS_H__" << endl << endl;
    layerHeader << "#include \"PacketDecoder.h\"" << endl << endl;
    layerHeader << "namespace opflexagent {" << endl;
    layerImpl << "#include \"PacketDecoderLayers.h\"" << endl << endl;
    layerImpl << "namespace opflexagent {" << endl << endl;
    layerImpl << "typedef PacketDecoderLayerFieldType PDF;" << endl << endl;
    while(char c1 = protoData.peek() == '*')  {
        protoData.getline(&buf[0], 200);
    }
    try {
        while(protoData.getline(&buf[0], 200)) {
            string str(buf.data());
            if(str.find('{') != string::npos) {
                break; 
            }
        }
    } catch (ios_base:: failure& fail) {
            cout << "No Starting brace {.Generation failed!" << endl;
    }

    int ret; 

    while((ret = ReadRecord(protoData,rec)) == 0) {
        string str;
        str = rec.layerName + "Layer";
        layerHeader << "class " << rec.layerName << "Layer: public PacketDecoderLayer {" << endl;
        layerHeader << "public:" << endl;
        layerHeader << "    " << rec.layerName << 
                "Layer():PacketDecoderLayer(\"" << rec.layerType << "\", " 
                << rec.layerKey<< ", \"" << rec.layerName <<"\", " 
                << rec.length << ", \""<< rec.nextLayerType << "\", \""
                << rec.optionLayerName << "\", " 
                << rec.layerTypeId << ", " << rec.layerId << ", " 
                << rec.nextLayerTypeId << ", "
                << rec.optionLayerTypeId << ", " 
                << rec.optionLayerId << "){};" << endl;
        layerHeader << "    " << "virtual int configure();" << endl;
        if(rec.optionLayerName != "none") {
            layerHeader << "    " << "virtual void getOptionLength(ParseInfo &p);" << endl;
        }
        layerImpl << "int " << rec.layerName << "Layer::configure() {" << endl;
        bool var_bytes = false;
        for(auto &fld: rec.fields) {
            layerImpl << "    addField(\"" << fld.fldName << "\", " << fld.bitLength << ", " << fld.offset << ", "<< "PDF::" << fld.fldTypeName << ", " << fld.flag << ", " << fld.nextKey << ", " << fld.length << ", " <<fld.scratchOffset << ");" << endl;
        if(fld.fldTypeName == "FLDTYPE_VARBYTES") {
            var_bytes = true;
        }
        }
        if(var_bytes) {
            layerHeader << "    " << "virtual uint32_t getVariableDataLength(uint32_t hdrLength);" << endl;
            layerHeader << "    " << "virtual uint32_t getVariableHeaderLength(uint32_t fldVal);" << endl;
        }
        layerHeader << "    " << "virtual void getFormatString(boost::format &fmtStr);" << endl;
	layerHeader << "};" << endl << endl;
        layerImpl << "    return 0;" <<endl;
        layerImpl << "}" << endl << endl;
        if(var_bytes) {
            layerImpl << "uint32_t " << str << "::getVariableDataLength(uint32_t hdrLength) {" << endl << "}" << endl << endl;
            layerImpl << "uint32_t " << str << "::getVariableHeaderLength(uint32_t fldVal) {" << endl << "}" << endl << endl;
        }
        if(rec.optionLayerName != "none") {
            layerImpl << "void " << str << "::getOptionLength(ParseInfo &p) {" << endl;
            layerImpl << "//Custom logic to compute option length goes here" << endl;
            layerImpl << "}" << endl << endl;
        }
        layerImpl << "void " << str << "::getFormatString(boost::format &fmtStr) {" << endl;
        layerImpl << "//Format string to print the layer goes here" << endl;
        layerImpl << "}" << endl << endl;
        decoderImpl << "shared_ptr<PacketDecoderLayer> " << "" << "sptr"<<rec.layerName << "(new " << str <<"());" << endl;

        decoderImpl << "sptr"<<rec.layerName << "->configure();" << endl;
        decoderImpl << "registerLayer(" <<"sptr" <<rec.layerName << ");"<< endl;
    } 

    if(ret == -2) {
        cout << "Generation successful!" << endl;
    } else {
        cout << "Generation failed!" << endl;
    } 

    layerImpl << "}" << endl;
    layerHeader << "}" << endl;
    layerHeader << "#endif /*__PACKETDECODERLAYER_H__*/" << endl;
}
