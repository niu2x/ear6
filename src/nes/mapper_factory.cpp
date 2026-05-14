#include "mapper_factory.h"
#include "mapper_000.h"
#include "mapper_001.h"
#include "mapper_002.h"
#include "mapper_003.h"
#include "mapper_004.h"
#include "mapper_007.h"
#include <cstdio>

namespace ear6::nes {

BaseMapper* MapperFactory::create(int mapper_number) {
    printf("[MapperFactory] Creating mapper %d\n", mapper_number);
    switch (mapper_number) {
        case 0:
            return new Mapper000();
        case 1:
            return new Mapper001();
        case 2:
            return new Mapper002();
        case 3:
            return new Mapper003();
        case 4:
            return new Mapper004();
        case 7:
            return new Mapper007();
        default:
            printf("[MapperFactory] WARNING: Unsupported mapper %d, falling back to NROM\n", mapper_number);
            return new Mapper000();
    }
}

} // namespace ear6::nes
