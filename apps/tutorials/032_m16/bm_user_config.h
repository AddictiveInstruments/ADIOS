/****************************************************************************
 *                                                                          *
 *                                                                          *
 *                                                                          *
 *                                                                          *
 ****************************************************************************/

#ifndef _BM_USER_CONFIG_H
#define _BM_USER_CONFIG_H

// ports and machines user definitions


#define BM_RTR_PORTS_NUM  56   //128 max
//num,  Node Name,      Port Name,    Node Id,  Port Id,
#define BM_RTR_USER_PORTS { \
{1,    "BM_RTR1",      "MIDI1",       0x00,     SPIM0      }, \
{2,    "BM_RTR1",      "MIDI2",       0x00,     SPIM1      }, \
{3,    "BM_RTR1",      "MIDI3",       0x00,     SPIM2      }, \
{4,    "BM_RTR1",      "MIDI4",       0x00,     SPIM3      }, \
{5,    "BM_RTR1",      "MIDI5",       0x00,     SPIM4      }, \
{6,    "BM_RTR1",      "MIDI6",       0x00,     SPIM5      }, \
{7,    "BM_RTR1",      "MIDI7",       0x00,     SPIM6      }, \
{8,    "BM_RTR1",      "MIDI8",       0x00,     SPIM7      }, \
{9,    "BM_RTR1",      "MIDI9",       0x00,     SPIM8      }, \
{10,   "BM_RTR1",      "MIDI10",      0x00,     SPIM9      }, \
{11,   "BM_RTR1",      "MIDI11",      0x00,     SPIM10     }, \
{12,   "BM_RTR1",      "MIDI12",      0x00,     SPIM11     }, \
{13,   "BM_RTR1",      "MIDI13",      0x00,     SPIM12     }, \
{14,   "BM_RTR1",      "MIDI14",      0x00,     SPIM13     }, \
{15,   "BM_RTR1",      "MIDI15",      0x00,     SPIM14     }, \
{16,   "BM_RTR1",      "MIDI16",      0x00,     SPIM15     }, \
{17,   "BM_RTR2",      "MIDI17",      0x01,     SPIM0      }, \
{18,   "BM_RTR2",      "MIDI18",      0x01,     SPIM1      }, \
{19,   "BM_RTR1",      "USB1",        0x00,     USB0       }, \
{20,   "BM_RTR1",      "USB2",        0x00,     USB1       }, \
{21,   "BM_RTR2",      "USB1",        0x01,     USB0       }, \
{22,   "BM_RTR2",      "USB2",        0x01,     USB1       }, \
{23,   "BM_CS1",       "USB1",        0x10,     USB0       }, \
{24,   "BM_CS1",       "USB2",        0x10,     USB1       }, \
{25,   "BM_RTR2",      "MIDI19",      0x01,     SPIM2      }, \
{26,   "BM_RTR2",      "MIDI20",      0x01,     SPIM3      }, \
{27,   "BM_RTR2",      "MIDI21",      0x01,     SPIM4      }, \
{28,   "BM_RTR2",      "MIDI22",      0x01,     SPIM5      }, \
{29,   "BM_RTR2",      "MIDI23",      0x01,     SPIM6      }, \
{30,   "BM_RTR2",      "MIDI24",      0x01,     SPIM7      }, \
{31,   "BM_RTR2",      "MIDI25",      0x01,     SPIM8      }, \
{32,   "BM_RTR2",      "MIDI26",      0x01,     SPIM9      }, \
{33,   "BM_RTR2",      "MIDI27",      0x01,     SPIM10     }, \
{34,   "BM_RTR2",      "MIDI28",      0x01,     SPIM11     }, \
{35,   "BM_RTR2",      "MIDI29",      0x01,     SPIM12     }, \
{36,   "BM_RTR2",      "MIDI30",      0x01,     SPIM13     }, \
{37,   "BM_RTR2",      "MIDI31",      0x01,     SPIM14     }, \
{38,   "BM_RTR2",      "MIDI32",      0x01,     SPIM15     }, \
{39,   "BM_RTR1",      "USB3",        0x00,     USB2       }, \
{40,   "BM_RTR1",      "USB4",        0x00,     USB3       }, \
{41,   "BM_RTR1",      "USB5",        0x00,     USB4       }, \
{42,   "BM_RTR1",      "USB6",        0x00,     USB5       }, \
{43,   "BM_RTR1",      "USB7",        0x00,     USB6       }, \
{44,   "BM_RTR1",      "USB8",        0x00,     USB7       }, \
{45,   "BM_RTR2",      "USB3",        0x01,     USB2       }, \
{46,   "BM_RTR2",      "USB4",        0x01,     USB3       }, \
{47,   "BM_RTR2",      "USB5",        0x01,     USB4       }, \
{48,   "BM_RTR2",      "USB6",        0x01,     USB5       }, \
{49,   "BM_RTR2",      "USB7",        0x01,     USB6       }, \
{50,   "BM_RTR2",      "USB8",        0x01,     USB7       }, \
{51,   "BM_CS1",       "USB3",        0x10,     USB2       }, \
{52,   "BM_CS1",       "USB4",        0x10,     USB3       }, \
{53,   "BM_CS1",       "USB5",        0x10,     USB4       }, \
{54,   "BM_CS1",       "USB6",        0x10,     USB5       }, \
{55,   "BM_CS1",       "USB7",        0x10,     USB6       }, \
{56,   "BM_CS1",       "USB7",        0x10,     USB7       }  \
}

#define BM_CS_MACHINE_NUM  13
//------------------------- Machine --------------------------------------    ---- MIDI Outputs ----        ---- MIDI Inputs ----       Loop
//num,  Node Name,      Brand,        Model,            DeviceId,             Number, Ports start at,       Number, Ports start at,     inhinbit
#define BM_CS_USER_MACHINES { \
{1,     "SUPERNOVA",    "Novation",   "Supernova IIR",  {0xff, 0xff, 0xff},   1,      1,                    1,      1,                  1         }, \
{2,     "MICROWAVE",    "Waldorf",    "Microwave",      {0xff, 0xff, 0xff},   1,      2,                    1,      2,                  1         }, \
{3,     "EX-8K",        "Korg",       "EX-8000",        {0xff, 0xff, 0xff},   1,      3,                    1,      3,                  1         }, \
{4,     "EX-800",       "Korg",       "EX-800",         {0xff, 0xff, 0xff},   1,      4,                    1,      4,                  1         }, \
{5,     "JUNO",         "Roland",     "Alpha Juno",     {0xff, 0xff, 0xff},   1,      5,                    1,      5,                  1         }, \
{6,     "MPC",          "Akai",       "MPC-X",          {0xff, 0xff, 0xff},   4,      6,                    1,      6,                  1         }, \
{7,     "SEQ",          "MIDIBox",    "MB-SEQv4+",      {0xff, 0xff, 0xff},   6,      10,                   2,      10,                 1         }, \
{8,     "SID",          "Midibox",    "SammichSid",     {0xff, 0xff, 0xff},   1,      16,                   1,      16,                 1         }, \
{9,     "TIA",          "Midibox",    "MB-TIA",         {0xff, 0xff, 0xff},   1,      17,                   1,      17,                 1         }, \
{10,    "MAQ",          "Doepfer",    "MAQ16/3",        {0xff, 0xff, 0xff},   1,      18,                   1,      18,                 1         }, \
{11,    "USB_BM_RTR1",  "USB",        "Driver",         {0xff, 0xff, 0xff},   2,      19,                   2,      19,                 1         }, \
{12,    "USB_BM_RTR2",  "USB",        "Driver",         {0xff, 0xff, 0xff},   2,      21,                   2,      21,                 1         }, \
{13,    "USB_BM_CS",    "USB",        "Driver",         {0xff, 0xff, 0xff},   2,      23,                   2,      23,                 1         }  \
}

//{"TR-909"           ,"TR-909"},
//{"TR-727"           ,"TR-727"},
//{"MACHINE-DRUM"     ,"MACHINE-DRUM"},
//{"YOCTO"            ,"YOCTO"},
//{"RX-7"             ,"RX-7"},
//{"PROTEUS"          ,"PROTEUS"},
//{"01V96"            ,"01V96"},
//{"A-70"             ,"A-70"},
//{"POLYMOON"         ,"POLYMOON"},
//{"EMPRESS REVERB"   ,"EMPRESS REVERB"},
//{"FLASHBACK x4"     ,"FLASHBACK x4"}#endif

#endif
