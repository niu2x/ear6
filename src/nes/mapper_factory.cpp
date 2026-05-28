#include "mapper_factory.h"
#include "mapper_000.h"
#include "mapper_001.h"
#include "mapper_002.h"
#include "mapper_003.h"
#include "mapper_004.h"
#include "mapper_005.h"
#include "mapper_006.h"
#include "mapper_007.h"
#include "mapper_009.h"
#include "mapper_011.h"
#include "mapper_013.h"
#include "mapper_015.h"
#include "mapper_016.h"
#include "mapper_032.h"
#include "mapper_033.h"
#include "mapper_034.h"
#include "mapper_038.h"
#include "mapper_039.h"
#include "mapper_058.h"
#include "mapper_060.h"
#include "mapper_062.h"
#include "mapper_066.h"
#include "mapper_067.h"
#include "mapper_070.h"
#include "mapper_078.h"
#include "mapper_079.h"
#include "mapper_086.h"
#include "mapper_087.h"
#include "mapper_089.h"
#include "mapper_092.h"
#include "mapper_093.h"
#include "mapper_094.h"
#include "mapper_061.h"
#include "mapper_065.h"
#include "mapper_133.h"
#include "mapper_140.h"
#include "mapper_143.h"
#include "mapper_145.h"
#include "mapper_148.h"
#include "mapper_149.h"
#include "mapper_156.h"
#include "mapper_180.h"
#include "mapper_184.h"
#include "mapper_185.h"
#include <cstdio>

namespace ear6::nes {

bool MapperFactory::is_supported(int mapper_number) {
    switch (mapper_number) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
        case 8: case 9: case 10: case 11: case 13: case 15: case 16: case 17:
        case 32: case 33: case 34: case 38: case 39: case 58: case 60: case 61: case 62:
        case 65:
        case 66: case 67: case 70: case 78: case 79:
        case 86: case 87: case 89: case 92: case 93: case 94:
        case 101: case 113: case 133: case 140: case 143: case 144:
        case 145: case 146: case 148: case 149:
        case 153: case 156: case 157: case 159:
        case 180: case 184: case 185:
            return true;
        default:
            return false;
    }
}

BaseMapper* MapperFactory::create(int mapper_number) {
    printf("[MapperFactory] Creating mapper %d\n", mapper_number);
    switch (mapper_number) {
        case 0: return new Mapper000();
        case 1: return new Mapper001();
        case 2: return new Mapper002();
        case 3: return new Mapper003();
        case 4: return new Mapper004();
        case 5: return new Mapper005();
        case 6: case 8: case 17: return new Mapper006();
        case 7: return new Mapper007();
        case 9: case 10: return new Mapper009();
        case 11: case 144: return new Mapper011();
        case 13: return new Mapper013();
        case 15: return new Mapper015();
        case 16: case 153: case 157: case 159: return new Mapper016();
        case 34: return new Mapper034();
        case 38: return new Mapper038();
        case 39: return new Mapper039();
        case 32: return new Mapper032();
        case 33: return new Mapper033();
        case 58: return new Mapper058();
        case 60: return new Mapper060();
        case 61: return new Mapper061();
        case 62: return new Mapper062();
        case 65: return new Mapper065();
        case 66: return new Mapper066();
        case 67: return new Mapper067();
        case 70: return new Mapper070();
        case 78: return new Mapper078();
        case 79: case 113: case 146: return new Mapper079();
        case 86: return new Mapper086();
        case 87: case 101: return new Mapper087();
        case 89: return new Mapper089();
        case 92: return new Mapper092();
        case 93: return new Mapper093();
        case 94: return new Mapper094();
        case 133: return new Mapper133();
        case 140: return new Mapper140();
        case 143: return new Mapper143();
        case 145: return new Mapper145();
        case 148: return new Mapper148();
        case 149: return new Mapper149();
        case 156: return new Mapper156();
        case 180: return new Mapper180();
        case 184: return new Mapper184();
        case 185: return new Mapper185();
        default:
            printf("[MapperFactory] WARNING: Unsupported mapper %d, falling back to NROM\n", mapper_number);
            return new Mapper000();
    }
}

} // namespace ear6::nes
