#ifndef BTVM_H
#define BTVM_H

#include <unordered_map>
#include <string>
#include <stack>
#include "vm/vm.h"
#include "format/btentry.h"
#include "btvmio.h"

class BTVM: public VM
{
    private:
        typedef std::unordered_map<uint64_t, uint32_t> ColorMap;

    public:
        BTVM(BTVMIO* btvmio = 0); 
        ~BTVM();
        virtual void parse(const std::string& code);
        virtual uint32_t color(const std::string& color) const;
        BTEntryList createTemplate();
		bool readIO(BTVMIO* btvmio);

    protected:
        virtual void print(const std::string& s);
        virtual void readValue(const VMValuePtr &vmvar, uint64_t size, bool seek);
        virtual void entryCreated(const BTEntryPtr& btentry);
        virtual uint64_t currentOffset() const;
        virtual uint32_t currentFgColor() const;
        virtual uint32_t currentBgColor() const;

    private:
        BTEntryPtr createEntry(const VMValuePtr& vmvalue, const BTEntryPtr &btparent);
        VMValuePtr readScalar(NCall* ncall, uint64_t bits, bool issigned);
        void initTypes();
        void initFunctions();
        void initColors();

    private: // Interface Functions
        static VMValuePtr vmPrintf(VM *self, NCall* ncall);
        static VMValuePtr vmSetBackColor(VM *self, NCall* ncall);
        static VMValuePtr vmSetForeColor(VM *self, NCall* ncall);
        static VMValuePtr vmWarning(VM *self, NCall* ncall);

    private: // I/O Functions
        static VMValuePtr vmFEof(VM *self, NCall* ncall);
        static VMValuePtr vmFileSize(VM *self, NCall* ncall);
        static VMValuePtr vmFTell(VM *self, NCall* ncall);
        static VMValuePtr vmReadBytes(VM *self, NCall* ncall);
        static VMValuePtr vmReadString(VM *self, NCall* ncall);
        static VMValuePtr vmReadInt(VM *self, NCall* ncall);
        static VMValuePtr vmReadInt64(VM *self, NCall* ncall);
        static VMValuePtr vmReadQuad(VM *self, NCall* ncall);
        static VMValuePtr vmReadShort(VM *self, NCall* ncall);
        static VMValuePtr vmReadUInt(VM *self, NCall* ncall);
        static VMValuePtr vmReadUInt64(VM *self, NCall* ncall);
        static VMValuePtr vmReadUQuad(VM *self, NCall* ncall);
        static VMValuePtr vmReadUShort(VM *self, NCall* ncall);
        static VMValuePtr vmLittleEndian(VM *self, NCall* ncall);
        static VMValuePtr vmBigEndian(VM *self, NCall* ncall);
        static VMValuePtr vmFSeek(VM *self, NCall* ncall);

    private: // String Functions
        static VMValuePtr vmStrlen(VM* self, NCall* ncall);

    private: // Math Functions
        static VMValuePtr vmCeil(VM* self, NCall* ncall);

    private: // Tool Functions
        static VMValuePtr vmFindAll(VM* self, NCall* ncall);

    private: // Non-Standard Functions
        static VMValuePtr vmBtvmTest(VM *self, NCall* ncall);

    private:
        std::unordered_map<std::string, uint32_t> _colors;
        std::list<Node*> _builtin;
        uint32_t _fgcolor;
        uint32_t _bgcolor;
        BTVMIO* _btvmio;
};

#endif // BTVM_H
