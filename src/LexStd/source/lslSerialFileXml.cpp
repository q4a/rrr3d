#include "stdafx.h"

#include "lslSerialFileXml.h"
#include "lslResource.h"

#define TIXML_USE_STL
#include "tinyxml.h"

namespace lsl
{

namespace
{

void SaveChunk(SerialNode& chunk, TiXmlNode& owner)
{
	TiXmlElement rootNode(chunk.GetName());
	std::string str;

	bool isParseValue = false;
	for (SerialNode::Attributes::const_iterator iter = chunk.GetAttributes().begin(); iter != chunk.GetAttributes().end(); ++iter)
	{
		iter->second->CastTo(&str);
		rootNode.SetAttribute(iter->first, str);
		//ищем имена элементов которые будут вставлены без оброботки
		if (!isParseValue)
			isParseValue = iter->first == SerialFileXML::cParse && str == "value";
	}

	chunk.GetValue().CastTo(&str);
	if (!str.empty())
	{
		if (isParseValue)
		{
			TiXmlElement text("tmp");
			text.Parse(str.c_str(), 0, TIXML_DEFAULT_ENCODING);
			rootNode.InsertEndChild(text);
		}
		else
		{
			TiXmlText text(str);
			rootNode.InsertEndChild(text);
		}
	}

	//пока не поддерживаетс€
	if (isParseValue && chunk.GetElements().Count() > 0)
	{
		LSL_LOG("oid SaveChunk(SerialNode& chunk, TiXmlNode& owner) 1");
		return;
	}

	SerialNode* child = chunk.FirstChild();
	while (child)
	{
		SaveChunk(*child, rootNode);
		child = child->NextNode();
	}

	owner.InsertEndChild(rootNode);
}

void LoadChunk(SerialNode& chunk, TiXmlNode& node)
{
	bool isParseValue = false;
	//аттрибуты
	TiXmlElement* elem = node.ToElement();
	if (elem)
	{
		TiXmlAttribute* attrib;
		attrib = elem->FirstAttribute();
		while (attrib)
		{
			chunk.AddAttribute(attrib->Name(), lsl::SIOTraits::ValueDesc())->AssignFromString(attrib->Value());
			if (!isParseValue)
				isParseValue = attrib->NameTStr() == SerialFileXML::cParse && attrib->ValueStr() == "value";
			attrib = attrib->Next();
		}
	}

	//≈сли нет дочерних узлов, то он €вл€етс€ пустым(возможны только атрибуты), поэтому выход
	if (node.NoChildren())
		return;

	if (isParseValue)
	{
		TiXmlPrinter printer;
		node.FirstChild()->Accept(&printer);
		chunk.SetValue(printer.Str());
	}
	else
	{
		TiXmlNode* iter = node.FirstChild();
		while (iter)
		{
			//Ёто узел
			if (iter->ToElement())
			{
				const char* pName = iter->Value();
				bool isTrue = pName == std::string("SEM");
				LoadChunk(*chunk.GetElements().Add(iter->Value()), *iter);
			}
			else
				//Ёто значение
				if (iter->ToText())
					chunk.SetValue(iter->ToText()->ValueTStr());

			iter = node.IterateChildren(iter);
		}
	}
}

}




void SerialFileXML::SaveNode(SerialNode& root, std::ostream& stream)
{
	TiXmlDocument doc("SerialFileXML");

	TiXmlDeclaration decl;
	decl.Parse("<?xml version='1.0' encoding='UTF-8'?>", 0, TIXML_ENCODING_UTF8);
	doc.InsertEndChild(decl);

	SaveChunk(root, doc);

	stream << doc;
}

void SerialFileXML::LoadNode(SerialNode& root, std::istream& stream)
{
	root.Clear();

	TiXmlDocument doc("SerialFileXML");
	stream >> doc;
	if (doc.NoChildren())
	{
		LSL_LOG("void SerialFileXML::LoadNode(SerialNode& root, std::istream& stream)")
		return;
	}

	TiXmlDeclaration* decl = doc.FirstChild()->ToDeclaration();
	TiXmlNode* node = decl != NULL ? decl->NextSibling() : doc.FirstChild();

	LoadChunk(root, *node);
}

void SerialFileXML::SaveNodeToFile(SerialNode& root, const std::string& fileName)
{
	std::ostream* stream = FileSystem::GetInstance()->NewOutStream(fileName, FileSystem::omText, 0);
	SaveNode(root, *stream);
	FileSystem::GetInstance()->FreeStream(stream);
}

void SerialFileXML::LoadNodeFromFile(SerialNode& root, const std::string& fileName)
{
	std::istream* stream = FileSystem::GetInstance()->NewInStream(fileName, FileSystem::omText, 0);
	try
	{
		LoadNode(root, *stream);
	}
	LSL_FINALLY(FileSystem::GetInstance()->FreeStream(stream);)
}

}