 #pragma once
#include "ZoneHeaderWriterAbstract.h"
namespace tecplot { namespace ___3933 { class ___37; struct ClassicZoneFileLocations; class FileWriterInterface; class ItemSetIterator; class ClassicZoneHeaderWriter : public ZoneHeaderWriterAbstract { public: ClassicZoneHeaderWriter( ItemSetIterator&                varIter, ___4636                     zone, ___4636                     ___341, ___37&                     ___36, ClassicZoneFileLocations const& ___4611); virtual ~ClassicZoneHeaderWriter(); virtual uint64_t sizeInFile(bool ___2002) const; virtual ___372 write(FileWriterInterface& fileWriter) const; private: ClassicZoneFileLocations const& m_zoneFileLocations; }; }}
