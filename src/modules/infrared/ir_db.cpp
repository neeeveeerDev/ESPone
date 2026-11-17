#include "ir_db.h"
#include "ir_sender_hw.h"

namespace ir_db {




static const IrCode kTvBGoneCodes[] = {

  { "Samsung 1", IR_PROTO_SAMSUNG, 0x0707, 0x02, 32 },
  { "Samsung 2", IR_PROTO_NEC, 0x07, 0x02, 32 },
  { "Samsung 3", IR_PROTO_SAMSUNG, 0x0707, 0x99, 32 },

  { "LG TV 1", IR_PROTO_NEC, 0x04, 0x08, 32 },
  { "LG TV 2", IR_PROTO_NEC, 0x00, 0x08, 32 },
  { "LG TV 3", IR_PROTO_NEC, 0x04, 0x10, 32 },

  { "Sony 1", IR_PROTO_SONY, 0x01, 0x15, 12 },
  { "Sony 2", IR_PROTO_SONY, 0x11, 0x15, 15 },
  { "Sony 3", IR_PROTO_SONY, 0xA1, 0x15, 20 },

  { "Philips 1", IR_PROTO_RC5, 0x00, 0x0C, 12 },
  { "Philips 2", IR_PROTO_RC6, 0x00, 0x0C, 20 },

  { "Panasonic 1", IR_PROTO_PANASONIC, 0x4004, 0x0100BCBD, 48 },
  { "Panasonic 2", IR_PROTO_PANASONIC, 0x4004, 0x01004C4D, 48 },

  { "Toshiba 1", IR_PROTO_NEC, 0x40, 0x12, 32 },
  { "Toshiba 2", IR_PROTO_NEC, 0x02, 0x02, 32 },

  { "Sharp 1", IR_PROTO_SHARP, 0x01, 0x41, 15 },
  { "Sharp 2", IR_PROTO_SHARP, 0x01, 0x11, 15 },

  { "TCL 1", IR_PROTO_NEC, 0x00, 0x12, 32 },
  { "TCL 2", IR_PROTO_NEC, 0x08, 0x12, 32 },

  { "Hisense 1", IR_PROTO_NEC, 0x00, 0x0C, 32 },
  { "Hisense 2", IR_PROTO_NEC, 0xF7, 0x0C, 32 },

  { "Xiaomi Mi", IR_PROTO_NEC, 0x00, 0x1A, 32 },

  { "Vizio 1", IR_PROTO_NEC, 0x04, 0x02, 32 },
  { "Vizio 2", IR_PROTO_NEC, 0x04, 0x0A, 32 },

  { "RCA 1", IR_PROTO_NEC, 0x08, 0x08, 32 },

  { "Sanyo 1", IR_PROTO_NEC, 0x14, 0x38, 32 },

  { "Hitachi 1", IR_PROTO_NEC, 0x50, 0x0C, 32 },

  { "JVC 1", IR_PROTO_JVC, 0x03, 0x38, 16 },

  { "Grundig 1", IR_PROTO_RC5, 0x00, 0x0C, 12 },

  { "Daewoo 1", IR_PROTO_NEC, 0x00, 0x40, 32 },

  { "Haier 1", IR_PROTO_NEC, 0x00, 0x48, 32 },

  { "Pioneer 1", IR_PROTO_NEC, 0xA5, 0x5A, 32 },

  { "Generic TV 1", IR_PROTO_NEC, 0x00, 0x45, 32 },
  { "Generic TV 2", IR_PROTO_NEC, 0x00, 0x46, 32 },
  { "Generic TV 3", IR_PROTO_NEC, 0x00, 0x47, 32 }
};

static const int kTvBGoneCount = sizeof(kTvBGoneCodes) / sizeof(kTvBGoneCodes[0]);

int getTvBGoneCount() {
  return kTvBGoneCount;
}

const IrCode& getTvBGoneCode(int index) {
  if (index >= 0 && index < kTvBGoneCount) {
    return kTvBGoneCodes[index];
  }
  return kTvBGoneCodes[0];
}




static const char* kCategoryNames[CAT_COUNT] = {
  "TV", "AC", "FAN", "PROJECTOR", "AUDIO"
};

const char* getCategoryName(UniversalCategory cat) {
  if (cat >= 0 && cat < CAT_COUNT) return kCategoryNames[cat];
  return "TV";
}

static const char* kTvButtons[] = { "Power", "Mute", "Vol +", "Vol -", "Ch +", "Ch -" };
static const char* kAcButtons[] = { "Power", "Temp +", "Temp -", "Mode", "Fan" };
static const char* kFanButtons[] = { "Power", "Speed +", "Speed -", "Oscillate", "Timer" };
static const char* kProjButtons[] = { "Power", "Source", "Menu", "Auto Sync" };
static const char* kAudioButtons[] = { "Power", "Mute", "Vol +", "Vol -", "Source" };

int getCategoryButtonCount(UniversalCategory cat) {
  switch (cat) {
    case CAT_TV: return sizeof(kTvButtons) / sizeof(kTvButtons[0]);
    case CAT_AC: return sizeof(kAcButtons) / sizeof(kAcButtons[0]);
    case CAT_FAN: return sizeof(kFanButtons) / sizeof(kFanButtons[0]);
    case CAT_PROJECTOR: return sizeof(kProjButtons) / sizeof(kProjButtons[0]);
    case CAT_AUDIO: return sizeof(kAudioButtons) / sizeof(kAudioButtons[0]);
    default: return 0;
  }
}

const char* getCategoryButtonName(UniversalCategory cat, int btnIndex) {
  switch (cat) {
    case CAT_TV: return kTvButtons[btnIndex];
    case CAT_AC: return kAcButtons[btnIndex];
    case CAT_FAN: return kFanButtons[btnIndex];
    case CAT_PROJECTOR: return kProjButtons[btnIndex];
    case CAT_AUDIO: return kAudioButtons[btnIndex];
    default: return "";
  }
}

void sendCategoryButton(UniversalCategory cat, int btnIndex) {
  if (cat == CAT_TV) {
    if (btnIndex == 0) {

      for (int i = 0; i < 12; i++) {
        ir_sender_hw::sendCode(kTvBGoneCodes[i]);
      }
    } else if (btnIndex == 1) {
      static const IrCode mutes[] = {
        { "Samsung Mute", IR_PROTO_SAMSUNG, 0x0707, 0x0E, 32 },
        { "LG Mute", IR_PROTO_NEC, 0x04, 0x09, 32 },
        { "Sony Mute", IR_PROTO_SONY, 0x01, 0x14, 12 },
        { "Philips Mute", IR_PROTO_RC5, 0x00, 0x0D, 12 },
        { "Panasonic Mute", IR_PROTO_PANASONIC, 0x4004, 0x01004C4D, 48 },
        { "TCL Mute", IR_PROTO_NEC, 0x00, 0x0D, 32 }
      };
      for (auto& c : mutes) ir_sender_hw::sendCode(c);
    } else if (btnIndex == 2) {
      static const IrCode volsUp[] = {
        { "Samsung Vol+", IR_PROTO_SAMSUNG, 0x0707, 0x07, 32 },
        { "LG Vol+", IR_PROTO_NEC, 0x04, 0x02, 32 },
        { "Sony Vol+", IR_PROTO_SONY, 0x01, 0x12, 12 },
        { "Philips Vol+", IR_PROTO_RC5, 0x00, 0x10, 12 },
        { "TCL Vol+", IR_PROTO_NEC, 0x00, 0x10, 32 }
      };
      for (auto& c : volsUp) ir_sender_hw::sendCode(c);
    } else if (btnIndex == 3) {
      static const IrCode volsDn[] = {
        { "Samsung Vol-", IR_PROTO_SAMSUNG, 0x0707, 0x0B, 32 },
        { "LG Vol-", IR_PROTO_NEC, 0x04, 0x03, 32 },
        { "Sony Vol-", IR_PROTO_SONY, 0x01, 0x13, 12 },
        { "Philips Vol-", IR_PROTO_RC5, 0x00, 0x11, 12 },
        { "TCL Vol-", IR_PROTO_NEC, 0x00, 0x11, 32 }
      };
      for (auto& c : volsDn) ir_sender_hw::sendCode(c);
    } else if (btnIndex == 4) {
      static const IrCode chUp[] = {
        { "Samsung Ch+", IR_PROTO_SAMSUNG, 0x0707, 0x12, 32 },
        { "LG Ch+", IR_PROTO_NEC, 0x04, 0x00, 32 },
        { "Sony Ch+", IR_PROTO_SONY, 0x01, 0x10, 12 }
      };
      for (auto& c : chUp) ir_sender_hw::sendCode(c);
    } else if (btnIndex == 5) {
      static const IrCode chDn[] = {
        { "Samsung Ch-", IR_PROTO_SAMSUNG, 0x0707, 0x10, 32 },
        { "LG Ch-", IR_PROTO_NEC, 0x04, 0x01, 32 },
        { "Sony Ch-", IR_PROTO_SONY, 0x01, 0x11, 12 }
      };
      for (auto& c : chDn) ir_sender_hw::sendCode(c);
    }
  } else if (cat == CAT_AC) {

    static const IrCode acCodes[] = {
      { "Gree AC", IR_PROTO_NEC, 0x00, 0x11, 32 },
      { "Midea AC", IR_PROTO_NEC, 0xB2, 0x4D, 32 },
      { "LG AC", IR_PROTO_LG, 0x88, 0x08, 28 },
      { "AUX AC", IR_PROTO_NEC, 0xC3, 0x28, 32 }
    };
    for (auto& c : acCodes) ir_sender_hw::sendCode(c);
  } else if (cat == CAT_FAN) {

    static const IrCode fanCodes[] = {
      { "Fan Power 1", IR_PROTO_NEC, 0x00, 0x01, 32 },
      { "Fan Power 2", IR_PROTO_NEC, 0x00, 0x08, 32 },
      { "Fan Power 3", IR_PROTO_NEC, 0x02, 0x05, 32 },
      { "Xiaomi Fan", IR_PROTO_NEC, 0xF1, 0x01, 32 }
    };
    for (auto& c : fanCodes) ir_sender_hw::sendCode(c);
  } else if (cat == CAT_PROJECTOR) {

    static const IrCode projCodes[] = {
      { "Epson Proj", IR_PROTO_NEC, 0x41, 0x51, 32 },
      { "BenQ Proj", IR_PROTO_NEC, 0x30, 0x08, 32 },
      { "Optoma Proj", IR_PROTO_NEC, 0x12, 0x01, 32 },
      { "ViewSonic Proj", IR_PROTO_NEC, 0x81, 0x06, 32 },
      { "Ricoh Proj", IR_PROTO_NEC, 0xCD82, 0x11, 32 },
      { "Ricoh Proj 2", IR_PROTO_NEC, 0xCD, 0x11, 32 }
    };
    for (auto& c : projCodes) ir_sender_hw::sendCode(c);
  } else if (cat == CAT_AUDIO) {

    static const IrCode audioCodes[] = {
      { "Sony Audio", IR_PROTO_SONY, 0x10, 0x15, 12 },
      { "Yamaha Audio", IR_PROTO_NEC, 0x7E, 0x0A, 32 },
      { "Pioneer Audio", IR_PROTO_NEC, 0xA5, 0x1A, 32 },
      { "Bose Audio", IR_PROTO_NEC, 0x5D, 0x01, 32 }
    };
    for (auto& c : audioCodes) ir_sender_hw::sendCode(c);
  }
}




static const char* kProjectorBrandNames[PROJ_COUNT] = {
  "All (Bruteforce)",
  "Epson",
  "ViewSonic",
  "BenQ",
  "Optoma",
  "Ricoh"
};

const char* getProjectorBrandName(ProjectorBrand b) {
  if (b >= 0 && b < PROJ_COUNT) return kProjectorBrandNames[b];
  return "Projector";
}

static const char* kAllBrandButtons[] = { "Power", "Source", "Menu", "Auto Sync" };

static const char* kEpsonButtons[] = {
  "Power", "Source", "Menu", "Enter", "Esc", "Keystone+", "Keystone-"
};

static const char* kViewSonicButtons[] = {
  "Power", "Source", "Menu", "Enter", "Blank", "Keystone+", "Keystone-"
};

static const char* kBenqButtons[] = {
  "Power", "Source", "Menu", "Enter", "Auto"
};

static const char* kOptomaButtons[] = {
  "Power", "Source", "Menu", "Enter", "Resync"
};

static const char* kRicohButtons[] = {
  "Power", "Source", "Menu", "Enter", "Auto", "Eco", "Keystone+", "Keystone-"
};

int getProjectorBrandButtonCount(ProjectorBrand b) {
  switch (b) {
    case PROJ_ALL: return sizeof(kAllBrandButtons) / sizeof(kAllBrandButtons[0]);
    case PROJ_EPSON: return sizeof(kEpsonButtons) / sizeof(kEpsonButtons[0]);
    case PROJ_VIEWSONIC: return sizeof(kViewSonicButtons) / sizeof(kViewSonicButtons[0]);
    case PROJ_BENQ: return sizeof(kBenqButtons) / sizeof(kBenqButtons[0]);
    case PROJ_OPTOMA: return sizeof(kOptomaButtons) / sizeof(kOptomaButtons[0]);
    case PROJ_RICOH: return sizeof(kRicohButtons) / sizeof(kRicohButtons[0]);
    default: return 0;
  }
}

const char* getProjectorBrandButtonName(ProjectorBrand b, int btnIndex) {
  switch (b) {
    case PROJ_ALL: return kAllBrandButtons[btnIndex];
    case PROJ_EPSON: return kEpsonButtons[btnIndex];
    case PROJ_VIEWSONIC: return kViewSonicButtons[btnIndex];
    case PROJ_BENQ: return kBenqButtons[btnIndex];
    case PROJ_OPTOMA: return kOptomaButtons[btnIndex];
    case PROJ_RICOH: return kRicohButtons[btnIndex];
    default: return "";
  }
}

void sendProjectorBrandButton(ProjectorBrand b, int btnIndex) {
  if (b == PROJ_ALL) {
    sendCategoryButton(CAT_PROJECTOR, btnIndex);
    return;
  }

  if (b == PROJ_EPSON) {


    switch (btnIndex) {
      case 0: {
        ir_sender_hw::sendCode(IrCode("Epson Power", IR_PROTO_NEC, 0x41, 0x51, 32));
        delay(120);
        ir_sender_hw::sendCode(IrCode("Epson Power", IR_PROTO_NEC, 0x41, 0x51, 32));
        break;
      }
      case 1:
        ir_sender_hw::sendCode(IrCode("Epson Source", IR_PROTO_NEC, 0x41, 0x45, 32));
        break;
      case 2:
        ir_sender_hw::sendCode(IrCode("Epson Menu", IR_PROTO_NEC, 0x41, 0x47, 32));
        break;
      case 3:
        ir_sender_hw::sendCode(IrCode("Epson Enter", IR_PROTO_NEC, 0x41, 0x5A, 32));
        break;
      case 4:
        ir_sender_hw::sendCode(IrCode("Epson Esc", IR_PROTO_NEC, 0x41, 0x41, 32));
        break;
      case 5:
        ir_sender_hw::sendCode(IrCode("Epson Key+", IR_PROTO_NEC, 0x41, 0x4D, 32));
        break;
      case 6:
        ir_sender_hw::sendCode(IrCode("Epson Key-", IR_PROTO_NEC, 0x41, 0x4E, 32));
        break;
    }
  } else if (b == PROJ_VIEWSONIC) {

    switch (btnIndex) {
      case 0: {
        ir_sender_hw::sendCode(IrCode("ViewSonic Power 1", IR_PROTO_NEC, 0x81, 0x06, 32));
        delay(120);
        ir_sender_hw::sendCode(IrCode("ViewSonic Power 1", IR_PROTO_NEC, 0x81, 0x06, 32));

        ir_sender_hw::sendCode(IrCode("ViewSonic Power 2", IR_PROTO_NEC, 0x84, 0x06, 32));
        break;
      }
      case 1:
        ir_sender_hw::sendCode(IrCode("ViewSonic Source", IR_PROTO_NEC, 0x81, 0x07, 32));
        break;
      case 2:
        ir_sender_hw::sendCode(IrCode("ViewSonic Menu", IR_PROTO_NEC, 0x81, 0x0B, 32));
        break;
      case 3:
        ir_sender_hw::sendCode(IrCode("ViewSonic Enter", IR_PROTO_NEC, 0x81, 0x0D, 32));
        break;
      case 4:
        ir_sender_hw::sendCode(IrCode("ViewSonic Blank", IR_PROTO_NEC, 0x81, 0x10, 32));
        break;
      case 5:
        ir_sender_hw::sendCode(IrCode("ViewSonic Key+", IR_PROTO_NEC, 0x81, 0x12, 32));
        break;
      case 6:
        ir_sender_hw::sendCode(IrCode("ViewSonic Key-", IR_PROTO_NEC, 0x81, 0x13, 32));
        break;
    }
  } else if (b == PROJ_BENQ) {

    switch (btnIndex) {
      case 0: {
        ir_sender_hw::sendCode(IrCode("BenQ Power", IR_PROTO_NEC, 0x30, 0x08, 32));
        delay(120);
        ir_sender_hw::sendCode(IrCode("BenQ Power", IR_PROTO_NEC, 0x30, 0x08, 32));
        break;
      }
      case 1:
        ir_sender_hw::sendCode(IrCode("BenQ Source", IR_PROTO_NEC, 0x30, 0x0D, 32));
        break;
      case 2:
        ir_sender_hw::sendCode(IrCode("BenQ Menu", IR_PROTO_NEC, 0x30, 0x0A, 32));
        break;
      case 3:
        ir_sender_hw::sendCode(IrCode("BenQ Enter", IR_PROTO_NEC, 0x30, 0x02, 32));
        break;
      case 4:
        ir_sender_hw::sendCode(IrCode("BenQ Auto", IR_PROTO_NEC, 0x30, 0x11, 32));
        break;
    }
  } else if (b == PROJ_OPTOMA) {

    switch (btnIndex) {
      case 0: {
        ir_sender_hw::sendCode(IrCode("Optoma Power", IR_PROTO_NEC, 0x12, 0x01, 32));
        delay(120);
        ir_sender_hw::sendCode(IrCode("Optoma Power", IR_PROTO_NEC, 0x12, 0x01, 32));
        break;
      }
      case 1:
        ir_sender_hw::sendCode(IrCode("Optoma Source", IR_PROTO_NEC, 0x12, 0x05, 32));
        break;
      case 2:
        ir_sender_hw::sendCode(IrCode("Optoma Menu", IR_PROTO_NEC, 0x12, 0x08, 32));
        break;
      case 3:
        ir_sender_hw::sendCode(IrCode("Optoma Enter", IR_PROTO_NEC, 0x12, 0x0B, 32));
        break;
      case 4:
        ir_sender_hw::sendCode(IrCode("Optoma Resync", IR_PROTO_NEC, 0x12, 0x14, 32));
        break;
    }
  } else if (b == PROJ_RICOH) {
    switch (btnIndex) {
      case 0: { // Power
        ir_sender_hw::sendCode(IrCode("Ricoh Power 1", IR_PROTO_NEC, 0xCD82, 0x11, 32));
        ir_sender_hw::sendCode(IrCode("Ricoh Power 2", IR_PROTO_NEC, 0xCD, 0x11, 32));
        delay(120);
        ir_sender_hw::sendCode(IrCode("Ricoh Power 1", IR_PROTO_NEC, 0xCD82, 0x11, 32));
        ir_sender_hw::sendCode(IrCode("Ricoh Power 2", IR_PROTO_NEC, 0xCD, 0x11, 32));
        ir_sender_hw::sendCode(IrCode("Ricoh Power Off", IR_PROTO_NEC, 0xCD82, 0x12, 32));
        break;
      }
      case 1: // Source
        ir_sender_hw::sendCode(IrCode("Ricoh Source 1", IR_PROTO_NEC, 0xCD82, 0x0B, 32));
        ir_sender_hw::sendCode(IrCode("Ricoh Source 2", IR_PROTO_NEC, 0xCD, 0x0B, 32));
        break;
      case 2: // Menu
        ir_sender_hw::sendCode(IrCode("Ricoh Menu 1", IR_PROTO_NEC, 0xCD82, 0x0E, 32));
        ir_sender_hw::sendCode(IrCode("Ricoh Menu 2", IR_PROTO_NEC, 0xCD, 0x0E, 32));
        break;
      case 3: // Enter
        ir_sender_hw::sendCode(IrCode("Ricoh Enter 1", IR_PROTO_NEC, 0xCD82, 0x1A, 32));
        ir_sender_hw::sendCode(IrCode("Ricoh Enter 2", IR_PROTO_NEC, 0xCD, 0x1A, 32));
        ir_sender_hw::sendCode(IrCode("Ricoh Enter 3", IR_PROTO_NEC, 0xCD82, 0x0D, 32));
        break;
      case 4: // Auto
        ir_sender_hw::sendCode(IrCode("Ricoh Auto 1", IR_PROTO_NEC, 0xCD82, 0x15, 32));
        ir_sender_hw::sendCode(IrCode("Ricoh Auto 2", IR_PROTO_NEC, 0xCD, 0x15, 32));
        break;
      case 5: // Eco
        ir_sender_hw::sendCode(IrCode("Ricoh Eco 1", IR_PROTO_NEC, 0xCD82, 0x14, 32));
        ir_sender_hw::sendCode(IrCode("Ricoh Eco 2", IR_PROTO_NEC, 0xCD, 0x14, 32));
        break;
      case 6: // Keystone+
        ir_sender_hw::sendCode(IrCode("Ricoh Key+ 1", IR_PROTO_NEC, 0xCD82, 0x16, 32));
        ir_sender_hw::sendCode(IrCode("Ricoh Key+ 2", IR_PROTO_NEC, 0xCD, 0x16, 32));
        break;
      case 7: // Keystone-
        ir_sender_hw::sendCode(IrCode("Ricoh Key- 1", IR_PROTO_NEC, 0xCD82, 0x17, 32));
        ir_sender_hw::sendCode(IrCode("Ricoh Key- 2", IR_PROTO_NEC, 0xCD, 0x17, 32));
        break;
    }
  }
}

}

