#ifndef LSL_SERIAL_FILE_XML
#define LSL_SERIAL_FILE_XML

#include "lslCommon.h"
#include "lslSerialization.h"

namespace lsl
{

class SerialFileXML: public SerialFile
{
public:
	virtual void SaveNode(SerialNode& root, std::ostream& stream);
	virtual void LoadNode(SerialNode& root, std::istream& stream);

	void SaveNodeToFile(SerialNode& root, const std::string& fileName);
	void LoadNodeFromFile(SerialNode& root, const std::string& fileName);
};

}

#endif