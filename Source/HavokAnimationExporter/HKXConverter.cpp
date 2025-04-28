#include "Pch.h"

// Code from HKXConverter

namespace
{
    class Endian 
    {
    public:
        static void swap(unsigned long long& x) 
        {
            x = _byteswap_uint64(x);
        }

        static void swap(unsigned int& x)
        {
            x = _byteswap_ulong(x);
        }

        static void swap(int& x) 
        {
            swap(reinterpret_cast<unsigned int&>(x));
        }

        static void swap(unsigned short& x)
        {
            x = _byteswap_ushort(x);
        }
    };

    class File
    {
    public:
        const void* data;
        size_t dataSize;
        size_t dataOffset;

        File(const void* data, size_t dataSize)
            : data(data), dataSize(dataSize), dataOffset(0) 
        {
        }

        size_t read(void* buffer, size_t size)
        {
            size_t available = dataSize - dataOffset;
            if (size > available)
                size = available;

            memcpy(buffer, reinterpret_cast<const unsigned char*>(data) + dataOffset, size);
            dataOffset += size;

            return size;
        }

        long tell() const 
        {
            return static_cast<long>(dataOffset);
        }

        void seek(long offset, int origin) 
        {
            switch (origin) {
            case SEEK_SET:
                if (offset < dataSize) {
                    dataOffset = offset;
                }
                else {
                    dataOffset = dataSize;
                }
                break;

            case SEEK_CUR:
                if ((dataOffset + offset) < dataSize) {
                    dataOffset += offset;
                }
                else {
                    dataOffset = dataSize;
                }
                break;

            case SEEK_END:
                if ((-offset) < dataSize) {
                    dataOffset = dataSize + offset;
                }
                else {
                    dataOffset = 0;
                }
                break;
            }
        }

        void align()
        {
            dataOffset = (dataOffset + 0xF) & ~0xF;
        }
    };

    enum
    {
        TYPE_VOID,
        TYPE_BOOL,
        TYPE_CHAR,
        TYPE_INT8,
        TYPE_UINT8,
        TYPE_INT16,
        TYPE_UINT16,
        TYPE_INT32,
        TYPE_UINT32,
        TYPE_INT64,
        TYPE_UINT64,
        TYPE_REAL,
        TYPE_VECTOR4,
        TYPE_QUATERNION,
        TYPE_MATRIX3,
        TYPE_ROTATION,
        TYPE_QSTRANSFORM,
        TYPE_MATRIX4,
        TYPE_TRANSFORM,
        TYPE_ZERO,
        TYPE_POINTER,
        TYPE_FUNCTION_POINTER,
        TYPE_ARRAY,
        TYPE_INPLACE_ARRAY,
        TYPE_ENUM,
        TYPE_STRUCT,
        TYPE_SIMPLE_ARRAY,
        TYPE_HOMOGENEOUS_ARRAY,
        TYPE_VARIANT,
        TYPE_CSTRING,
        TYPE_ULONG,
        TYPE_FLAGS,
        TYPE_HALF,
        TYPE_STRING_POINTER
    };

    class HavokClassName
    {
    public:
        unsigned int tag;
        std::string name;

        HavokClassName()
        {
        }
    };

    class HavokTypeMember
    {
    public:
        unsigned char tag[2];
        unsigned short arraySize;
        unsigned short structType;
        unsigned short offset;
        unsigned int structureAddress;
        std::string name;
        std::string structure;

        HavokTypeMember()
        {
        }
    };

    class HavokEnum
    {
    public:
        unsigned int id;
        std::string name;
    };

    class HavokType
    {
    public:
        unsigned int objectSize;
        std::string name;
        std::string className;
        unsigned int describedVersion;
        unsigned int numImplementedInterfaces;
        unsigned int declaredEnums;
        unsigned int address;

        std::vector<HavokTypeMember> members;
        std::vector<HavokEnum> enums;
        std::vector<std::vector<HavokEnum>> subEnums;
        std::vector<std::string> subEnumNames;

        HavokType* parent;
        unsigned int parentAddress;

        void reset()
        {
            members.clear();
            enums.clear();
            subEnums.clear();
            subEnumNames.clear();
            name = "";

            parent = NULL;
            parentAddress = 0;
        }
    };

    class HavokLink
    {
    public:
        unsigned int type;
        unsigned int address1;
        unsigned int address2;
        HavokType* typeParent;
        HavokType* typeNode;
    };

    class HavokPointer
    {
    public:
        unsigned absAddress;
        unsigned targetAddress;
    };

    struct HavokPackfileHeader
    {
        int magic[2];
        int userTag;
        int fileVersion;
        unsigned char layoutRules[4];
        int numSections;
        int contentsSectionIndex;
        int contentsSectionOffset;
        int contentsClassNameSectionIndex;
        int contentsClassNameSectionOffset;
        char contentsVersion[16];
        int flags;
        int pad[1];
    };

    struct HavokPackfileSectionHeader
    {
        char sectionTag[19];
        char nullByte;
        unsigned int absoluteDataStart;
        unsigned int localFixupsOffset;
        unsigned int globalFixupsOffset;
        unsigned int virtualFixupsOffset;
        unsigned int exportsOffset;
        unsigned int importsOffset;
        unsigned int endOffset;

        void endianSwap()
        {
            Endian::swap(absoluteDataStart);
            Endian::swap(localFixupsOffset);
            Endian::swap(globalFixupsOffset);
            Endian::swap(virtualFixupsOffset);
            Endian::swap(exportsOffset);
            Endian::swap(importsOffset);
            Endian::swap(endOffset);
        }
    };

    struct HKXConverterImpl
    {
        std::vector<unsigned char> out;
        unsigned int classNameGlobalAddress = 0;

        void endianSwap(int i, int sz)
        {
            char t = 0;

            for (int c = 0; c < sz / 2; c++)
            {
                t = out[i + c];
                out[i + c] = out[i + (sz - c) - 1];
                out[i + (sz - c) - 1] = t;
            }
        }

        std::list<HavokClassName> classNames;
        std::list<HavokType> types;
        std::list<HavokLink> typeLinks;
        std::list<HavokLink> dataLinks;
        std::list<HavokPointer> dataPointers;
        std::list<HavokPointer> dataGlobalPointers;

        void endianSwap(HavokPackfileHeader& header)
        {
            header.userTag = 0;
            Endian::swap(header.fileVersion);
            header.layoutRules[0] = 4;
            header.layoutRules[1] = 1;
            header.layoutRules[2] = 0;
            header.layoutRules[3] = 1;

            Endian::swap(header.numSections);
            Endian::swap(header.contentsSectionIndex);
            Endian::swap(header.contentsSectionOffset);

            Endian::swap(header.contentsClassNameSectionIndex);
            Endian::swap(header.contentsClassNameSectionOffset);

            Endian::swap(header.flags);

            endianSwap(8, 4);
            endianSwap(12, 4);
            endianSwap(20, 4);
            endianSwap(24, 4);
            endianSwap(28, 4);
            endianSwap(32, 4);
            endianSwap(36, 4);

            out[17] = 1;
        }

        void convertElement(File* fp, int mainType)
        {
            if ((mainType == TYPE_INT16) || (mainType == TYPE_UINT16) || (mainType == TYPE_HALF))
            {
                endianSwap(fp->tell(), 2);
                fp->seek(2, SEEK_CUR);
            }
            else if ((mainType == TYPE_INT32) || (mainType == TYPE_UINT32) || (mainType == TYPE_REAL) ||
                (mainType == TYPE_POINTER) || (mainType == TYPE_ULONG) || (mainType == TYPE_STRING_POINTER) ||
                (mainType == TYPE_CSTRING))
            {
                endianSwap(fp->tell(), 4);
                fp->seek(4, SEEK_CUR);
            }
            else if (mainType == TYPE_VARIANT)
            {
                endianSwap(fp->tell(), 4);
                endianSwap(fp->tell() + 4, 4);
                fp->seek(8, SEEK_CUR);
            }
            else if ((mainType == TYPE_INT64) || (mainType == TYPE_UINT64))
            {
                endianSwap(fp->tell(), 8);
                fp->seek(8, SEEK_CUR);
            }
            else if ((mainType == TYPE_VECTOR4) || (mainType == TYPE_QUATERNION))
            {
                endianSwap(fp->tell(), 4);
                endianSwap(fp->tell() + 4, 4);
                endianSwap(fp->tell() + 8, 4);
                endianSwap(fp->tell() + 12, 4);
                fp->seek(16, SEEK_CUR);
            }
            else if ((mainType == TYPE_MATRIX3) || (mainType == TYPE_ROTATION) || (mainType == TYPE_QSTRANSFORM))
            {
                for (int j = 0; j < 12; j++)
                    endianSwap(fp->tell() + j * 4, 4);

                fp->seek(4 * 12, SEEK_CUR);
            }
            else if ((mainType == TYPE_MATRIX4) || (mainType == TYPE_TRANSFORM))
            {
                for (int j = 0; j < 16; j++)
                    endianSwap(fp->tell() + j * 4, 4);

                fp->seek(4 * 16, SEEK_CUR);
            }
            else if ((mainType == TYPE_ARRAY))
            {
                endianSwap(fp->tell(), 4);
                endianSwap(fp->tell() + 4, 4);
                endianSwap(fp->tell() + 8, 4);
            }
            else
                fp->seek(1, SEEK_CUR);
        }

        void convertStructure(File* fp, std::string typeName)
        {
            unsigned int start = fp->tell();
            bool found = false;

            for (std::list<HavokType>::iterator it = types.begin(); it != types.end(); it++)
            {
                if ((*it).name != typeName)
                    continue;
                unsigned int arrayOffset = start + (*it).objectSize;
                found = true;

                if ((*it).parent)
                    convertStructure(fp, (*it).parent->name);

                for (int i = 0; i < (*it).members.size(); i++)
                {
                    int mainType = (*it).members[i].tag[0];
                    int subType = (*it).members[i].tag[1];

                    fp->seek(start + (*it).members[i].offset, SEEK_SET);

                    if (mainType == TYPE_ENUM)
                        mainType = subType;

                    if (mainType == TYPE_STRUCT)
                        convertStructure(fp, (*it).members[i].structure);

                    else if (mainType == TYPE_POINTER)
                    {
                        endianSwap(fp->tell(), 4);

                        for (std::list<HavokPointer>::iterator it2 = dataPointers.begin(); it2 != dataPointers.end(); it2++)
                        {
                            if (fp->tell() == (*it2).absAddress)
                            {
                                fp->seek((*it2).targetAddress, SEEK_SET);
                                break;
                            }
                        }
                    }
                    else if ((mainType == TYPE_ARRAY) || (mainType == TYPE_SIMPLE_ARRAY))
                    {
                        unsigned int count = 0;

                        fp->seek(4, SEEK_CUR);
                        fp->read(&count, sizeof(unsigned int));
                        Endian::swap(count);

                        endianSwap(fp->tell() - 8, 4);
                        endianSwap(fp->tell() - 4, 4);

                        if (mainType == TYPE_ARRAY)
                            endianSwap(fp->tell(), 4);

                        if (count == 0)
                        {
                            continue;
                        }

                        for (std::list<HavokPointer>::iterator it2 = dataPointers.begin(); it2 != dataPointers.end(); it2++)
                        {
                            if ((fp->tell() - 8) == (*it2).absAddress)
                            {
                                fp->seek((*it2).targetAddress, SEEK_SET);
                                break;
                            }
                        }

                        unsigned int newAddr = fp->tell();
                        unsigned int size = 1;

                        for (std::list<HavokType>::iterator it2 = types.begin(); it2 != types.end(); it2++)
                        {
                            if ((*it2).name == (*it).members[i].structure)
                            {
                                size = (*it2).objectSize;
                                break;
                            }
                        }

                        for (int j = 0; j < count; j++)
                        {
                            if (subType == TYPE_STRUCT)
                            {
                                fp->seek(newAddr + j * size, SEEK_SET);
                                convertStructure(fp, (*it).members[i].structure);
                            }
                            else
                            {
                                if (subType == TYPE_POINTER)
                                {
                                }
                                else
                                {
                                    convertElement(fp, subType);
                                }
                            }
                        }
                    }
                    else
                    {
                        unsigned int count = (*it).members[i].arraySize;
                        if (count == 0)
                            count = 1;

                        for (int j = 0; j < count; j++)
                        {
                            convertElement(fp, mainType);
                        }
                    }
                }
            }
        }

        void readData(HavokPackfileSectionHeader& header, File* fp)
        {
            fp->seek(header.absoluteDataStart, SEEK_SET);

            if (!strcmp(header.sectionTag, "__classnames__"))
            {
                HavokClassName className;

                classNameGlobalAddress = header.absoluteDataStart;

                int i = 0;
                while (className.tag != (unsigned int)-1)
                {
                    className.name = "";

                    fp->read(&className.tag, sizeof(unsigned int));
                    Endian::swap(className.tag);
                    endianSwap(fp->tell() - 4, 4);

                    if ((className.tag == (unsigned int)-1) || (className.tag == 0))
                    {
                        break;
                    }
                    else
                    {
                        fp->seek(1, SEEK_CUR);
                        char c = 0;
                        for (int i = 0; i < 256; i++)
                        {
                            fp->read(&c, sizeof(char));
                            if (!c)
                                break;

                            className.name += c;
                        }

                        classNames.push_back(className);
                    }

                    i++;
                }
            }

            if (!strcmp(header.sectionTag, "__types__"))
            {
                int i = 0;
                while (true)
                {
                    fp->seek(header.absoluteDataStart + header.localFixupsOffset + i * 4, SEEK_SET);

                    unsigned int address = 0;
                    fp->read(&address, sizeof(unsigned int));
                    Endian::swap(address);
                    if (address == (unsigned int)-1)
                        break;

                    endianSwap(fp->tell() - 4, 4);

                    if (fp->tell() >= header.absoluteDataStart + header.globalFixupsOffset)
                        break;

                    i++;
                }

                i = 0;
                while (true)
                {
                    fp->seek(header.absoluteDataStart + header.globalFixupsOffset + i * 12, SEEK_SET);

                    unsigned int address = 0;
                    unsigned int type = 0;
                    unsigned int metaAddress = 0;
                    fp->read(&address, sizeof(unsigned int));
                    Endian::swap(address);
                    if (address == (unsigned int)-1)
                        break;

                    fp->read(&type, sizeof(unsigned int));
                    Endian::swap(type);

                    fp->read(&metaAddress, sizeof(unsigned int));
                    Endian::swap(metaAddress);

                    HavokLink link;
                    link.address1 = header.absoluteDataStart + address;
                    link.address2 = header.absoluteDataStart + metaAddress;
                    link.type = type;
                    typeLinks.push_back(link);

                    endianSwap(fp->tell() - 12, 4);
                    endianSwap(fp->tell() - 8, 4);
                    endianSwap(fp->tell() - 4, 4);

                    if (fp->tell() >= header.absoluteDataStart + header.virtualFixupsOffset)
                        break;

                    i++;
                }

                i = 0;
                while (true)
                {
                    fp->seek(header.absoluteDataStart + header.virtualFixupsOffset + i * 12, SEEK_SET);

                    unsigned int address = 0;
                    unsigned int nameAddress = 0;
                    std::string typeName = "";

                    fp->read(&address, sizeof(unsigned int));
                    fp->seek(4, SEEK_CUR);

                    if (((address == 0) && (i != 0)) || (address == (unsigned int)-1))
                        break;

                    fp->read(&nameAddress, sizeof(unsigned int));

                    Endian::swap(address);
                    Endian::swap(nameAddress);

                    endianSwap(fp->tell() - 4, 4);
                    endianSwap(fp->tell() - 8, 4);
                    endianSwap(fp->tell() - 12, 4);

                    fp->seek(classNameGlobalAddress + nameAddress, SEEK_SET);
                    for (int k = 0; k < 256; k++)
                    {
                        char c = 0;
                        fp->read(&c, sizeof(char));
                        if (c)
                            typeName += c;
                        else
                            break;
                    }

                    fp->seek(header.absoluteDataStart + address, SEEK_SET);

                    HavokType type;
                    type.reset();
                    type.address = header.absoluteDataStart + address;
                    type.className = typeName;

                    if ((typeName != "hkClass") && (typeName != "hkClassEnum"))
                    {
                        i++;
                        continue;
                    }

                    fp->seek(4, SEEK_CUR);

                    for (std::list<HavokLink>::iterator it = typeLinks.begin(); it != typeLinks.end(); it++)
                    {
                        if ((*it).address1 == fp->tell())
                        {
                            type.parentAddress = (*it).address2;
                            break;
                        }
                    }

                    fp->seek(4, SEEK_CUR);
                    fp->read(&type.objectSize, sizeof(unsigned int));
                    Endian::swap(type.objectSize);
                    endianSwap(fp->tell() - 4, 4);

                    fp->read(&type.numImplementedInterfaces, sizeof(unsigned int));
                    Endian::swap(type.numImplementedInterfaces);
                    endianSwap(fp->tell() - 4, 4);

                    fp->seek(4, SEEK_CUR);

                    fp->read(&type.declaredEnums, sizeof(unsigned int));
                    Endian::swap(type.declaredEnums);
                    endianSwap(fp->tell() - 4, 4);

                    fp->seek(4, SEEK_CUR);

                    unsigned int memberNum = 0;
                    fp->read(&memberNum, sizeof(unsigned int));
                    Endian::swap(memberNum);
                    endianSwap(fp->tell() - 4, 4);

                    if (typeName == "hkClass")
                    {
                        fp->seek(12, SEEK_CUR);
                        fp->read(&type.describedVersion, sizeof(unsigned int));
                        Endian::swap(type.describedVersion);
                        endianSwap(fp->tell() - 4, 4);
                    }

                    char c = 0;
                    type.name = "";
                    for (int k = 0; k < 256; k++)
                    {
                        fp->read(&c, sizeof(char));
                        if (!c)
                            break;

                        type.name += c;
                    }

                    fp->align();

                    if (typeName == "hkClass")
                    {
                        std::vector<int> subsSizes;
                        for (int j = 0; j < type.declaredEnums; j++)
                        {
                            fp->seek(8, SEEK_CUR);
                            int sz = 0;
                            fp->read(&sz, sizeof(int));
                            Endian::swap(sz);
                            endianSwap(fp->tell() - 4, 4);

                            fp->seek(8, SEEK_CUR);

                            subsSizes.push_back(sz);
                        }

                        for (int j = 0; j < type.declaredEnums; j++)
                        {
                            std::string nm = "";
                            for (int k = 0; k < 256; k++)
                            {
                                char c = 0;
                                fp->read(&c, sizeof(char));

                                if (c)
                                    nm += c;
                                else
                                    break;
                            }
                            fp->align();

                            std::vector<HavokEnum> subs;
                            subs.clear();
                            type.subEnumNames.push_back(nm);

                            fp->align();

                            HavokEnum en;
                            for (int x = 0; x < subsSizes[j]; x++)
                            {
                                fp->read(&en.id, sizeof(unsigned int));
                                Endian::swap(en.id);
                                endianSwap(fp->tell() - 4, 4);

                                en.name = "";
                                fp->seek(4, SEEK_CUR);

                                subs.push_back(en);
                            }

                            fp->align();

                            for (int x = 0; x < subsSizes[j]; x++)
                            {
                                for (int k = 0; k < 256; k++)
                                {
                                    char c = 0;
                                    fp->read(&c, sizeof(char));

                                    if (c)
                                        subs[x].name += c;
                                    else
                                        break;
                                }

                                fp->align();

                                type.subEnums.push_back(subs);
                            }
                        }

                        HavokTypeMember typeMember;
                        for (int j = 0; j < memberNum; j++)
                        {
                            fp->seek(4, SEEK_CUR);

                            typeMember.structureAddress = 0;

                            for (std::list<HavokLink>::iterator it = typeLinks.begin(); it != typeLinks.end(); it++)
                            {
                                if ((*it).address1 == fp->tell())
                                {
                                    typeMember.structureAddress = (*it).address2;
                                    break;
                                }
                            }

                            fp->seek(8, SEEK_CUR);

                            fp->read(typeMember.tag, 2);

                            fp->read(&typeMember.arraySize, sizeof(unsigned short));
                            Endian::swap(typeMember.arraySize);

                            fp->read(&typeMember.structType, sizeof(unsigned short));
                            Endian::swap(typeMember.structType);

                            fp->read(&typeMember.offset, sizeof(unsigned short));
                            Endian::swap(typeMember.offset);

                            endianSwap(fp->tell() - 6, 2);
                            endianSwap(fp->tell() - 4, 2);
                            endianSwap(fp->tell() - 2, 2);

                            typeMember.name = "";
                            typeMember.structure = "";

                            type.members.push_back(typeMember);
                            fp->seek(4, SEEK_CUR);
                        }

                        for (int j = 0; j < memberNum; j++)
                        {
                            type.members[j].name = "";
                            for (int k = 0; k < 256; k++)
                            {
                                char c = 0;
                                fp->read(&c, sizeof(char));

                                if (c)
                                    type.members[j].name += c;
                                else
                                    break;
                            }

                            fp->align();
                        }
                    }
                    else
                    {
                        HavokEnum en;

                        for (int j = 0; j < type.objectSize; j++)
                        {
                            fp->read(&en.id, sizeof(unsigned int));
                            Endian::swap(en.id);
                            endianSwap(fp->tell() - 4, 4);

                            en.name = "";

                            fp->seek(4, SEEK_CUR);

                            type.enums.push_back(en);
                        }
                        fp->align();

                        for (int j = 0; j < type.objectSize; j++)
                        {
                            for (int k = 0; k < 256; k++)
                            {
                                char c = 0;
                                fp->read(&c, sizeof(char));

                                if (c)
                                    type.enums[j].name += c;
                                else
                                    break;
                            }

                            fp->align();
                        }
                    }

                    types.push_back(type);
                    i++;
                }

                i = 0;
                for (std::list<HavokLink>::iterator it = typeLinks.begin(); it != typeLinks.end(); it++)
                {
                    (*it).typeParent = NULL;
                    (*it).typeNode = NULL;

                    i++;
                }

                int j = 0;
                for (std::list<HavokType>::iterator it = types.begin(); it != types.end(); it++)
                {
                    if ((*it).className != "hkClass")
                        continue;

                    if ((*it).parentAddress)
                    {
                        for (std::list<HavokType>::iterator it2 = types.begin(); it2 != types.end(); it2++)
                        {
                            if ((*it2).address == (*it).parentAddress)
                            {
                                (*it).parent = &(*it2);
                                break;
                            }
                        }
                    }

                    for (int x = 0; x < (*it).members.size(); x++)
                    {
                        if ((*it).members[x].structureAddress)
                        {
                            for (std::list<HavokType>::iterator it2 = types.begin(); it2 != types.end(); it2++)
                            {
                                if ((*it2).address == (*it).members[x].structureAddress)
                                {
                                    (*it).members[x].structure = (*it2).name;
                                    break;
                                }
                            }
                        }
                    }

                    j++;
                }
            }

            if (!strcmp(header.sectionTag, "__data__"))
            {
                int i = 0;
                while (true)
                {
                    fp->seek(header.absoluteDataStart + header.localFixupsOffset + i * 8, SEEK_SET);

                    unsigned int address = 0;
                    unsigned int address2 = 0;
                    fp->read(&address, sizeof(unsigned int));
                    Endian::swap(address);
                    if (address == (unsigned int)-1)
                        break;

                    fp->read(&address2, sizeof(unsigned int));
                    Endian::swap(address2);

                    endianSwap(fp->tell() - 8, 4);
                    endianSwap(fp->tell() - 4, 4);

                    HavokPointer pointer;

                    pointer.absAddress = address + header.absoluteDataStart;
                    pointer.targetAddress = address2 + header.absoluteDataStart;

                    dataPointers.push_back(pointer);

                    if (fp->tell() >= header.absoluteDataStart + header.globalFixupsOffset)
                        break;

                    i++;
                }

                i = 0;
                while (true)
                {
                    fp->seek(header.absoluteDataStart + header.globalFixupsOffset + i * 12, SEEK_SET);

                    unsigned int address = 0;
                    unsigned int type = 0;
                    unsigned int metaAddress = 0;
                    fp->read(&address, sizeof(unsigned int));
                    Endian::swap(address);
                    if (address == (unsigned int)-1)
                        break;

                    fp->read(&type, sizeof(unsigned int));
                    Endian::swap(type);

                    fp->read(&metaAddress, sizeof(unsigned int));
                    Endian::swap(metaAddress);

                    endianSwap(fp->tell() - 12, 4);
                    endianSwap(fp->tell() - 8, 4);
                    endianSwap(fp->tell() - 4, 4);

                    HavokPointer pointer;
                    pointer.absAddress = address + header.absoluteDataStart;
                    pointer.targetAddress = metaAddress + header.absoluteDataStart;
                    dataGlobalPointers.push_back(pointer);

                    if (fp->tell() >= header.absoluteDataStart + header.virtualFixupsOffset)
                        break;

                    i++;
                }

                i = 0;
                while (true)
                {
                    fp->seek(header.absoluteDataStart + header.virtualFixupsOffset + i * 12, SEEK_SET);

                    unsigned int address = 0;
                    unsigned int nameAddress = 0;
                    std::string typeName = "";

                    fp->read(&address, sizeof(unsigned int));
                    fp->seek(4, SEEK_CUR);
                    Endian::swap(address);
                    if (address == (unsigned int)-1)
                        break;

                    fp->read(&nameAddress, sizeof(unsigned int));

                    endianSwap(fp->tell() - 4, 4);
                    endianSwap(fp->tell() - 8, 4);
                    endianSwap(fp->tell() - 12, 4);

                    Endian::swap(nameAddress);
                    unsigned back = fp->tell();

                    fp->seek(classNameGlobalAddress + nameAddress, SEEK_SET);
                    for (int k = 0; k < 256; k++)
                    {
                        char c = 0;
                        fp->read(&c, sizeof(char));
                        if (c)
                            typeName += c;
                        else
                            break;
                    }

                    fp->seek(header.absoluteDataStart + address, SEEK_SET);

                    convertStructure(fp, typeName);

                    if (back >= header.absoluteDataStart + header.exportsOffset)
                        break;

                    i++;
                }
            }
        }
    };
}

std::vector<unsigned char> endianSwapHKX(const void* data, size_t dataSize)
{
    File file(data, dataSize);

    HKXConverterImpl impl;
    impl.out.resize(file.dataSize);
    file.read(impl.out.data(), impl.out.size());

    HavokPackfileHeader header;
    file.seek(0, SEEK_SET);
    file.read(&header, sizeof(HavokPackfileHeader));
    impl.endianSwap(header);

    for (int i = 0; i < header.numSections; i++)
    {
        HavokPackfileSectionHeader sectionHeader;
        file.read(&sectionHeader, sizeof(HavokPackfileSectionHeader));
        sectionHeader.endianSwap();

        impl.endianSwap(file.tell() - 28, 4);
        impl.endianSwap(file.tell() - 24, 4);
        impl.endianSwap(file.tell() - 20, 4);
        impl.endianSwap(file.tell() - 16, 4);
        impl.endianSwap(file.tell() - 12, 4);
        impl.endianSwap(file.tell() - 8, 4);
        impl.endianSwap(file.tell() - 4, 4);

        unsigned int address = file.tell();
        impl.readData(sectionHeader, &file);
        file.seek(address, SEEK_SET);
    }

    return std::move(impl.out);
}