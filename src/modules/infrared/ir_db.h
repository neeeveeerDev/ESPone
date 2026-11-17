#pragma once
#include <stdint.h>

namespace ir_db {

enum IrProtocolType : uint8_t {
  IR_PROTO_NEC = 0,
  IR_PROTO_SAMSUNG,
  IR_PROTO_SONY,
  IR_PROTO_RC5,
  IR_PROTO_RC6,
  IR_PROTO_PANASONIC,
  IR_PROTO_LG,
  IR_PROTO_SHARP,
  IR_PROTO_JVC
};

struct IrCode {
  const char* name;
  IrProtocolType proto;
  uint16_t address;
  uint32_t command;
  uint8_t bits;
  uint64_t rawData;

  IrCode() : name(nullptr), proto(IR_PROTO_NEC), address(0), command(0), bits(0), rawData(0) {}
  IrCode(const char* n, IrProtocolType p, uint16_t a, uint32_t c, uint8_t b, uint64_t r = 0)
    : name(n), proto(p), address(a), command(c), bits(b), rawData(r) {}
};


int getTvBGoneCount();
const IrCode& getTvBGoneCode(int index);


enum UniversalCategory {
  CAT_TV = 0,
  CAT_AC,
  CAT_FAN,
  CAT_PROJECTOR,
  CAT_AUDIO,
  CAT_COUNT
};


enum ProjectorBrand {
  PROJ_ALL = 0,
  PROJ_EPSON,
  PROJ_VIEWSONIC,
  PROJ_BENQ,
  PROJ_OPTOMA,
  PROJ_RICOH,
  PROJ_COUNT
};

const char* getCategoryName(UniversalCategory cat);
int getCategoryButtonCount(UniversalCategory cat);
const char* getCategoryButtonName(UniversalCategory cat, int btnIndex);
void sendCategoryButton(UniversalCategory cat, int btnIndex);


const char* getProjectorBrandName(ProjectorBrand b);
int getProjectorBrandButtonCount(ProjectorBrand b);
const char* getProjectorBrandButtonName(ProjectorBrand b, int btnIndex);
void sendProjectorBrandButton(ProjectorBrand b, int btnIndex);

}

