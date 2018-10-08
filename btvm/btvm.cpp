#include "btvm.h"
#include "vm/vm_functions.h"
#include "btvm_types.h"
#include "../bt_lexer.h"
#include <iostream>
#include <cstdio>
#include <cmath>

#define ColorizeFail(s) "\x1b[31m" << s << "\x1b[0m"
#define ColorizeOk(s)   "\x1b[32m" << s << "\x1b[0m"

// Parser interface
void* BTParserAlloc(void* (*mallocproc)(size_t));
void BTParserFree(void* p, void (*freeproc)(void*));
void BTParser(void* yyp, int yymajor, BTLexer::Token* yyminor, BTVM* btvm);

BTVM::BTVM(BTVMIO *btvmio): VM(), _fgcolor(ColorInvalid), _bgcolor(ColorInvalid), _btvmio(btvmio)
{
    this->initTypes();
    this->initFunctions();
    this->initColors();
}

BTVM::~BTVM()
{
    for(auto it = this->_builtin.begin(); it != this->_builtin.end(); it++)
        delete *it;
}

void BTVM::parse(const string &code)
{
    VM::parse(code);

    BTLexer lexer(code.c_str());
    std::list<BTLexer::Token> tokens = lexer.lex();

    if(tokens.size() <= 0)
        return;

    void* parser = BTParserAlloc(&malloc);

    for(auto it = tokens.begin(); it != tokens.end(); it++)
    {
        if(this->state == VMState::Error)
            break;

        BTParser(parser, it->type, &(*it), this);
    }

    BTParser(parser, 0, NULL, this);
    BTParserFree(parser, &free);
}

BTEntryList BTVM::createTemplate()
{
    BTEntryList btfmt;

    if(this->state == VMState::NoState)
    {
        for(auto it = this->allocations.begin(); it != this->allocations.end(); it++)
            btfmt.push_back(this->createEntry(*it, NULL));
    }
    else
        this->allocations.clear();

    return btfmt;
}

bool BTVM::readIO(BTVMIO* btvmio)
{
	this->clear();
	_btvmio = btvmio;

	return this->interpret(this->getAST()) != 0;
}

void BTVM::print(const string &s)
{
    cout << s;
}

void BTVM::readValue(const VMValuePtr& vmvar, uint64_t size, bool seek)
{
    if(!seek)
    {
        IO_NoSeek(this->_btvmio);
        this->_btvmio->read(vmvar, size);
        return;
    }

    this->_btvmio->read(vmvar, size);
}

void BTVM::entryCreated(const BTEntryPtr &btentry)
{
    VMUnused(btentry);
}

uint64_t BTVM::currentOffset() const
{
    return this->_btvmio->offset();
}

uint32_t BTVM::color(const string &color) const
{
    auto it = this->_colors.find(color);

    if(it == this->_colors.end())
        return ColorInvalid;

    return it->second;
}

uint32_t BTVM::currentFgColor() const
{
    return this->_fgcolor;
}

uint32_t BTVM::currentBgColor() const
{

    return this->_bgcolor;
}

BTEntryPtr BTVM::createEntry(const VMValuePtr &vmvalue, const BTEntryPtr& btparent)
{
    BTEntryPtr btentry = std::make_shared<BTEntry>(vmvalue, this->_btvmio->endianness());
    btentry->location = BTLocation(vmvalue->value_offset, this->sizeOf(vmvalue));
    btentry->parent = btparent;

    if(vmvalue->is_array() || node_is(vmvalue->value_typedef, NStruct))
    {
        for(auto it = vmvalue->m_value.begin(); it != vmvalue->m_value.end(); it++)
            btentry->children.push_back(this->createEntry(*it, btentry));
    }

    this->entryCreated(btentry);
    return btentry;
}

VMValuePtr BTVM::readScalar(NCall *ncall, uint64_t bits, bool issigned)
{
    VMValuePtr pos;

    if(ncall->arguments.size() > 1)
        return this->error("Expected 0 or 1 arguments, " + std::to_string(ncall->arguments.size()) + " given");

    IO_NoSeek(this->_btvmio);

    if(ncall->arguments.size() == 1)
    {
        pos = this->interpret(ncall->arguments.front());

        if(!pos->is_scalar())
            return this->typeError(pos, "scalar");

        this->_btvmio->seek(pos->ui_value);
    }

    VMValuePtr vmvalue = VMValue::allocate(bits, issigned, false);
    this->_btvmio->read(vmvalue, this->sizeOf(vmvalue));
    return vmvalue;
}

void BTVM::initTypes()
{
    Node* n = BTVMTypes::buildTFindResults();
    this->_builtin.push_back(n);
    this->declare(n);
}

void BTVM::initFunctions()
{
    // Interface Functions: https://www.sweetscape.com/010editor/manual/FuncInterface.htm
    this->functions["Printf"]        = &BTVM::vmPrintf;
    this->functions["SetBackColor"]  = &BTVM::vmSetBackColor;
    this->functions["SetForeColor"]  = &BTVM::vmSetForeColor;
    this->functions["Warning"]       = &BTVM::vmWarning;

    // I/O Functions: https://www.sweetscape.com/010editor/manual/FuncIO.htm
    this->functions["FEof"]          = &BTVM::vmFEof;
    this->functions["FileSize"]      = &BTVM::vmFileSize;
    this->functions["FTell"]         = &BTVM::vmFTell;
    this->functions["FSeek"]         = &BTVM::vmFSeek;
    this->functions["ReadInt"]       = &BTVM::vmReadInt;
    this->functions["ReadInt64"]     = &BTVM::vmReadInt64;
    this->functions["ReadQuad"]      = &BTVM::vmReadQuad;
    this->functions["ReadShort"]     = &BTVM::vmReadShort;
    this->functions["ReadUInt"]      = &BTVM::vmReadUInt;
    this->functions["ReadUInt64"]    = &BTVM::vmReadUInt64;
    this->functions["ReadUQuad"]     = &BTVM::vmReadUQuad;
    this->functions["ReadUShort"]    = &BTVM::vmReadUShort;
    this->functions["ReadBytes"]     = &BTVM::vmReadBytes;
    this->functions["ReadString"]    = &BTVM::vmReadString;
    this->functions["ReadUShort"]    = &BTVM::vmReadUShort;
    this->functions["LittleEndian"]  = &BTVM::vmLittleEndian;
    this->functions["BigEndian"]     = &BTVM::vmBigEndian;

    // String Functions: https://www.sweetscape.com/010editor/manual/FuncString.htm
    this->functions["Strlen"]        = &BTVM::vmStrlen;

    // Math Functions: https://www.sweetscape.com/010editor/manual/FuncMath.htm
    this->functions["Ceil"]          = &BTVM::vmCeil;

    // Tool Functions: https://www.sweetscape.com/010editor/manual/FuncTools.htm
    this->functions["FindAll"]       = &BTVM::vmFindAll;

    // Non-Standard Functions
    this->functions["__btvm_test__"] = &BTVM::vmBtvmTest; // Non-standard BTVM function for unit testing
}

void BTVM::initColors()
{
    this->_colors["cBlack"]    = 0x00000000;
    this->_colors["cRed"]      = 0x000000FF;
    this->_colors["cDkRed"]    = 0x00000080;
    this->_colors["cLtRed"]    = 0x008080FF;
    this->_colors["cGreen"]    = 0x0000FF00;
    this->_colors["cDkGreen"]  = 0x00008000;
    this->_colors["cLtGreen"]  = 0x0080FF80;
    this->_colors["cBlue"]     = 0x00FF0000;
    this->_colors["cDkBlue"]   = 0x00800000;
    this->_colors["cLtBlue"]   = 0x00FF8080;
    this->_colors["cPurple"]   = 0x00FF00FF;
    this->_colors["cDkPurple"] = 0x00800080;
    this->_colors["cLtPurple"] = 0x00FFE0FF;
    this->_colors["cAqua"]     = 0x00FFFF00;
    this->_colors["cDkAqua"]   = 0x00808000;
    this->_colors["cLtAqua"]   = 0x00FFFFE0;
    this->_colors["cYellow"]   = 0x0000FFFF;
    this->_colors["cDkYellow"] = 0x00008080;
    this->_colors["cLtYellow"] = 0x0080FFFF;
    this->_colors["cDkGray"]   = 0x00404040;
    this->_colors["cGray"]     = 0x00808080;
    this->_colors["cSilver"]   = 0x00C0C0C0;
    this->_colors["cLtGray"]   = 0x00E0E0E0;
    this->_colors["cWhite"]    = 0x00FFFFFF;
    this->_colors["cNone"]     = 0xFFFFFFFF;
}

VMValuePtr BTVM::vmPrintf(VM *self, NCall *ncall)
{
    VMValuePtr format = self->interpret(ncall->arguments.front());
    VMFunctions::ValueList args;

    if(ncall->arguments.size() > 1)
    {
        for(auto it = ncall->arguments.begin() + 1; it != ncall->arguments.end(); it++)
            args.push_back(self->interpret(*it));
    }

    static_cast<BTVM*>(self)->print(VMFunctions::format_string(format, args));
    return VMValuePtr();
}

VMValuePtr BTVM::vmSetBackColor(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 1)
        return self->argumentError(ncall, 1);

    if(!node_is(ncall->arguments[0], NIdentifier))
        return self->typeError(ncall->arguments[0], node_s_typename(NIdentifier));

    NIdentifier* nid = static_cast<NIdentifier*>(ncall->arguments[0]);
    static_cast<BTVM*>(self)->_bgcolor = self->color(nid->value);
    return VMValuePtr();
}

VMValuePtr BTVM::vmSetForeColor(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 1)
        return self->argumentError(ncall, 1);

    if(!node_is(ncall->arguments[0], NIdentifier))
        return self->typeError(ncall->arguments[0], node_s_typename(NIdentifier));

    NIdentifier* nid = static_cast<NIdentifier*>(ncall->arguments.front());
    static_cast<BTVM*>(self)->_fgcolor = self->color(nid->value);;
    return VMValuePtr();
}

VMValuePtr BTVM::vmLittleEndian(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 0)
        return self->argumentError(ncall, 0);

    static_cast<BTVM*>(self)->_btvmio->setLittleEndian();
    return VMValuePtr();
}

VMValuePtr BTVM::vmBigEndian(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 0)
        return self->argumentError(ncall, 0);

    static_cast<BTVM*>(self)->_btvmio->setBigEndian();
    return VMValuePtr();
}

VMValuePtr BTVM::vmFSeek(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 1)
        return self->argumentError(ncall, 1);

    VMValuePtr vmvalue = VMValue::copy_value(*self->interpret(ncall->arguments.front()));

    if(!vmvalue->is_scalar())
        return self->typeError(vmvalue, "scalar");

    BTVM* btvm = static_cast<BTVM*>(self);
    uint64_t offset = *vmvalue->value_ref<uint64_t>();

    if(offset >= btvm->_btvmio->size())
        return VMValue::allocate_literal(static_cast<int64_t>(-1));

    btvm->_btvmio->seek(offset);
    return VMValue::allocate_literal(static_cast<int64_t>(0));
}

VMValuePtr BTVM::vmStrlen(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 1)
        return self->argumentError(ncall, 1);

    VMValuePtr vmvalue = self->interpret(ncall->arguments.front());

    if(!vmvalue->is_string())
        return self->typeError(vmvalue, "string");

    return VMValue::allocate_literal(static_cast<int64_t>(vmvalue->length()));
}

VMValuePtr BTVM::vmCeil(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 1)
        return self->argumentError(ncall, 1);

    VMValuePtr vmvalue = VMValue::copy_value(*self->interpret(ncall->arguments.front()));

    if(!vmvalue->is_scalar())
        return self->typeError(vmvalue, "scalar");

    vmvalue->d_value = std::ceil(vmvalue->d_value);
    return vmvalue;
}

VMValuePtr BTVM::vmFindAll(VM *self, NCall *ncall)
{
    VMUnused(self);
    VMUnused(ncall);

    cout << "FindAll(): Not implemented";
    return VMValuePtr();
}

VMValuePtr BTVM::vmWarning(VM *self, NCall *ncall)
{
    static_cast<BTVM*>(self)->print("WARNING: ");
    return BTVM::vmPrintf(self, ncall);
}

VMValuePtr BTVM::vmBtvmTest(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 1)
        return self->argumentError(ncall, 1);

    VMValuePtr testres = self->interpret(ncall->arguments.front());

    if(*testres)
        cout << ColorizeOk("OK") << endl;
    else
        cout << ColorizeFail("FAIL") << endl;

    return testres;
}

VMValuePtr BTVM::vmFEof(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 0)
        return self->argumentError(ncall, 0);

    BTVM* btvm = static_cast<BTVM*>(self);
    return VMValue::allocate_literal(btvm->_btvmio->atEof());
}

VMValuePtr BTVM::vmFileSize(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 0)
        return self->argumentError(ncall, 0);

    BTVM* btvm = static_cast<BTVM*>(self);
    return VMValue::allocate_literal(btvm->_btvmio->size());
}

VMValuePtr BTVM::vmFTell(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 0)
        return self->argumentError(ncall, 0);

    BTVM* btvm = static_cast<BTVM*>(self);
    return VMValue::allocate_literal(btvm->_btvmio->offset());
}

VMValuePtr BTVM::vmReadBytes(VM *self, NCall *ncall)
{
    if(ncall->arguments.size() != 3)
        return self->argumentError(ncall, 3);

    BTVM* btvm = static_cast<BTVM*>(self);
    VMValuePtr vmbuffer = self->interpret(ncall->arguments[0]);

    if(!vmbuffer->is_array() && !vmbuffer->is_string())
        return self->typeError(vmbuffer, "array or string");

    VMValuePtr vmpos = self->interpret(ncall->arguments[1]);

    if(!vmpos->is_scalar())
        return self->typeError(vmpos, "scalar");

    VMValuePtr vmn = self->interpret(ncall->arguments[2]);

    if(!vmn->is_scalar())
        return self->typeError(vmn, "scalar");

    IO_NoSeek(btvm->_btvmio);

    btvm->_btvmio->seek(vmpos->ui_value);
    btvm->_btvmio->read(vmbuffer, vmn->ui_value);
    return VMValuePtr();
}

VMValuePtr BTVM::vmReadString(VM *self, NCall *ncall)
{
    if((ncall->arguments.size() < 1) || (ncall->arguments.size() > 2))
        return self->error("Expected 1 or 2 arguments, " + std::to_string(ncall->arguments.size()) + " given");

    BTVM* btvm = static_cast<BTVM*>(self);
    VMValuePtr vmpos = self->interpret(ncall->arguments[0]);

    if(!vmpos->is_scalar())
        return self->typeError(vmpos, "scalar");

    int32_t maxlen = -1;

    if(ncall->arguments.size() == 2)
    {
        VMValuePtr vmmaxlen = self->interpret(ncall->arguments[1]);

        if(vmmaxlen->is_scalar())
            return self->typeError(vmmaxlen, "scalar");

        maxlen = *vmmaxlen->value_ref<int32_t>();
    }

    IO_NoSeek(btvm->_btvmio);

    VMValuePtr vmvalue = VMValue::allocate(VMValueType::String);
    btvm->_btvmio->seek(vmpos->ui_value);
    btvm->_btvmio->readString(vmvalue, maxlen);
    return vmvalue;
}

VMValuePtr BTVM::vmReadInt(VM *self, NCall *ncall)    { return static_cast<BTVM*>(self)->readScalar(ncall, 32, true);  }
VMValuePtr BTVM::vmReadInt64(VM *self, NCall *ncall)  { return static_cast<BTVM*>(self)->readScalar(ncall, 64, true);  }
VMValuePtr BTVM::vmReadQuad(VM *self, NCall *ncall)   { return static_cast<BTVM*>(self)->vmReadInt64(self, ncall);     }
VMValuePtr BTVM::vmReadShort(VM *self, NCall *ncall)  { return static_cast<BTVM*>(self)->readScalar(ncall, 16, true);  }
VMValuePtr BTVM::vmReadUInt(VM *self, NCall *ncall)   { return static_cast<BTVM*>(self)->readScalar(ncall, 32, false); }
VMValuePtr BTVM::vmReadUInt64(VM *self, NCall *ncall) { return static_cast<BTVM*>(self)->readScalar(ncall, 64, false); }
VMValuePtr BTVM::vmReadUQuad(VM *self, NCall *ncall)  { return static_cast<BTVM*>(self)->vmReadUInt64(self, ncall);    }
VMValuePtr BTVM::vmReadUShort(VM *self, NCall *ncall) { return static_cast<BTVM*>(self)->readScalar(ncall, 16, false); }
