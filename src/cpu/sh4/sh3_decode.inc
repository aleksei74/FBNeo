// Original SH opcode decode, shared by switch and flat-dispatch paths.
static inline SH3_DEC_RET SH3_DEC_NAME(execute_one_0000)(const UINT16 opcode)
{
	switch(opcode & 0xff)
	{
		// 0x00
		case 0x00:  SH3_DEC_OP(NOP); break;
		case 0x10:  SH3_DEC_OP(NOP); break;
		case 0x20:  SH3_DEC_OP(NOP); break;
		case 0x30:  SH3_DEC_OP(NOP); break;
		case 0x40:  SH3_DEC_OP(NOP); break;
		case 0x50:  SH3_DEC_OP(NOP); break;
		case 0x60:  SH3_DEC_OP(NOP); break;
		case 0x70:  SH3_DEC_OP(NOP); break;
		case 0x80:  SH3_DEC_OP(NOP); break;
		case 0x90:  SH3_DEC_OP(NOP); break;
		case 0xa0:  SH3_DEC_OP(NOP); break;
		case 0xb0:  SH3_DEC_OP(NOP); break;
		case 0xc0:  SH3_DEC_OP(NOP); break;
		case 0xd0:  SH3_DEC_OP(NOP); break;
		case 0xe0:  SH3_DEC_OP(NOP); break;
		case 0xf0:  SH3_DEC_OP(NOP); break;
		// 0x10
		case 0x01:  SH3_DEC_OP(NOP); break;
		case 0x11:  SH3_DEC_OP(NOP); break;
		case 0x21:  SH3_DEC_OP(NOP); break;
		case 0x31:  SH3_DEC_OP(NOP); break;
		case 0x41:  SH3_DEC_OP(NOP); break;
		case 0x51:  SH3_DEC_OP(NOP); break;
		case 0x61:  SH3_DEC_OP(NOP); break;
		case 0x71:  SH3_DEC_OP(NOP); break;
		case 0x81:  SH3_DEC_OP(NOP); break;
		case 0x91:  SH3_DEC_OP(NOP); break;
		case 0xa1:  SH3_DEC_OP(NOP); break;
		case 0xb1:  SH3_DEC_OP(NOP); break;
		case 0xc1:  SH3_DEC_OP(NOP); break;
		case 0xd1:  SH3_DEC_OP(NOP); break;
		case 0xe1:  SH3_DEC_OP(NOP); break;
		case 0xf1:  SH3_DEC_OP(NOP); break;
		// 0x20
		case 0x02:  SH3_DEC_OP(STCSR); break;
		case 0x12:  SH3_DEC_OP(STCGBR); break;
		case 0x22:  SH3_DEC_OP(STCVBR); break;
		case 0x32:  SH3_DEC_OP(STCSSR); break;
		case 0x42:  SH3_DEC_OP(STCSPC); break;
		case 0x52:  SH3_DEC_OP(NOP); break;
		case 0x62:  SH3_DEC_OP(NOP); break;
		case 0x72:  SH3_DEC_OP(NOP); break;
		case 0x82:  SH3_DEC_OP(STCRBANK); break;
		case 0x92:  SH3_DEC_OP(STCRBANK); break;
		case 0xa2:  SH3_DEC_OP(STCRBANK); break;
		case 0xb2:  SH3_DEC_OP(STCRBANK); break;
		case 0xc2:  SH3_DEC_OP(STCRBANK); break;
		case 0xd2:  SH3_DEC_OP(STCRBANK); break;
		case 0xe2:  SH3_DEC_OP(STCRBANK); break;
		case 0xf2:  SH3_DEC_OP(STCRBANK); break;
		// 0x30
		case 0x03:  SH3_DEC_OP(BSRF); break;
		case 0x13:  SH3_DEC_OP(NOP); break;
		case 0x23:  SH3_DEC_OP(BRAF); break;
		case 0x33:  SH3_DEC_OP(NOP); break;
		case 0x43:  SH3_DEC_OP(NOP); break;
		case 0x53:  SH3_DEC_OP(NOP); break;
		case 0x63:  SH3_DEC_OP(NOP); break;
		case 0x73:  SH3_DEC_OP(NOP); break;
		case 0x83:  SH3_DEC_OP(PREFM); break;
		case 0x93:  SH3_DEC_OP(TODO); break;
		case 0xa3:  SH3_DEC_OP(TODO); break;
		case 0xb3:  SH3_DEC_OP(TODO); break;
		case 0xc3:  SH3_DEC_OP(MOVCAL); break;
		case 0xd3:  SH3_DEC_OP(NOP); break;
		case 0xe3:  SH3_DEC_OP(NOP); break;
		case 0xf3:  SH3_DEC_OP(NOP); break;
		// 0x40
		case 0x04:  SH3_DEC_OP(MOVBS0); break;
		case 0x14:  SH3_DEC_OP(MOVBS0); break;
		case 0x24:  SH3_DEC_OP(MOVBS0); break;
		case 0x34:  SH3_DEC_OP(MOVBS0); break;
		case 0x44:  SH3_DEC_OP(MOVBS0); break;
		case 0x54:  SH3_DEC_OP(MOVBS0); break;
		case 0x64:  SH3_DEC_OP(MOVBS0); break;
		case 0x74:  SH3_DEC_OP(MOVBS0); break;
		case 0x84:  SH3_DEC_OP(MOVBS0); break;
		case 0x94:  SH3_DEC_OP(MOVBS0); break;
		case 0xa4:  SH3_DEC_OP(MOVBS0); break;
		case 0xb4:  SH3_DEC_OP(MOVBS0); break;
		case 0xc4:  SH3_DEC_OP(MOVBS0); break;
		case 0xd4:  SH3_DEC_OP(MOVBS0); break;
		case 0xe4:  SH3_DEC_OP(MOVBS0); break;
		case 0xf4:  SH3_DEC_OP(MOVBS0); break;
		// 0x50
		case 0x05:  SH3_DEC_OP(MOVWS0); break;
		case 0x15:  SH3_DEC_OP(MOVWS0); break;
		case 0x25:  SH3_DEC_OP(MOVWS0); break;
		case 0x35:  SH3_DEC_OP(MOVWS0); break;
		case 0x45:  SH3_DEC_OP(MOVWS0); break;
		case 0x55:  SH3_DEC_OP(MOVWS0); break;
		case 0x65:  SH3_DEC_OP(MOVWS0); break;
		case 0x75:  SH3_DEC_OP(MOVWS0); break;
		case 0x85:  SH3_DEC_OP(MOVWS0); break;
		case 0x95:  SH3_DEC_OP(MOVWS0); break;
		case 0xa5:  SH3_DEC_OP(MOVWS0); break;
		case 0xb5:  SH3_DEC_OP(MOVWS0); break;
		case 0xc5:  SH3_DEC_OP(MOVWS0); break;
		case 0xd5:  SH3_DEC_OP(MOVWS0); break;
		case 0xe5:  SH3_DEC_OP(MOVWS0); break;
		case 0xf5:  SH3_DEC_OP(MOVWS0); break;
		// 0x60
		case 0x06:  SH3_DEC_OP(MOVLS0); break;
		case 0x16:  SH3_DEC_OP(MOVLS0); break;
		case 0x26:  SH3_DEC_OP(MOVLS0); break;
		case 0x36:  SH3_DEC_OP(MOVLS0); break;
		case 0x46:  SH3_DEC_OP(MOVLS0); break;
		case 0x56:  SH3_DEC_OP(MOVLS0); break;
		case 0x66:  SH3_DEC_OP(MOVLS0); break;
		case 0x76:  SH3_DEC_OP(MOVLS0); break;
		case 0x86:  SH3_DEC_OP(MOVLS0); break;
		case 0x96:  SH3_DEC_OP(MOVLS0); break;
		case 0xa6:  SH3_DEC_OP(MOVLS0); break;
		case 0xb6:  SH3_DEC_OP(MOVLS0); break;
		case 0xc6:  SH3_DEC_OP(MOVLS0); break;
		case 0xd6:  SH3_DEC_OP(MOVLS0); break;
		case 0xe6:  SH3_DEC_OP(MOVLS0); break;
		case 0xf6:  SH3_DEC_OP(MOVLS0); break;
		// 0x70
		case 0x07:  SH3_DEC_OP(MULL); break;
		case 0x17:  SH3_DEC_OP(MULL); break;
		case 0x27:  SH3_DEC_OP(MULL); break;
		case 0x37:  SH3_DEC_OP(MULL); break;
		case 0x47:  SH3_DEC_OP(MULL); break;
		case 0x57:  SH3_DEC_OP(MULL); break;
		case 0x67:  SH3_DEC_OP(MULL); break;
		case 0x77:  SH3_DEC_OP(MULL); break;
		case 0x87:  SH3_DEC_OP(MULL); break;
		case 0x97:  SH3_DEC_OP(MULL); break;
		case 0xa7:  SH3_DEC_OP(MULL); break;
		case 0xb7:  SH3_DEC_OP(MULL); break;
		case 0xc7:  SH3_DEC_OP(MULL); break;
		case 0xd7:  SH3_DEC_OP(MULL); break;
		case 0xe7:  SH3_DEC_OP(MULL); break;
		case 0xf7:  SH3_DEC_OP(MULL); break;
		// 0x80
		case 0x08:  SH3_DEC_OP(CLRT); break;
		case 0x18:  SH3_DEC_OP(SETT); break;
		case 0x28:  SH3_DEC_OP(CLRMAC); break;
		case 0x38:  SH3_DEC_OP(TODO); break;
		case 0x48:  SH3_DEC_OP(CLRS); break;
		case 0x58:  SH3_DEC_OP(SETS); break;
		case 0x68:  SH3_DEC_OP(NOP); break;
		case 0x78:  SH3_DEC_OP(NOP); break;
		case 0x88:  SH3_DEC_OP(CLRT); break;
		case 0x98:  SH3_DEC_OP(SETT); break;
		case 0xa8:  SH3_DEC_OP(CLRMAC); break;
		case 0xb8:  SH3_DEC_OP(TODO); break;
		case 0xc8:  SH3_DEC_OP(CLRS); break;
		case 0xd8:  SH3_DEC_OP(SETS); break;
		case 0xe8:  SH3_DEC_OP(NOP); break;
		case 0xf8:  SH3_DEC_OP(NOP); break;
		// 0x90
		case 0x09:  SH3_DEC_OP(NOP); break;
		case 0x19:  SH3_DEC_OP(DIV0U); break;
		case 0x29:  SH3_DEC_OP(MOVT); break;
		case 0x39:  SH3_DEC_OP(NOP); break;
		case 0x49:  SH3_DEC_OP(NOP); break;
		case 0x59:  SH3_DEC_OP(DIV0U); break;
		case 0x69:  SH3_DEC_OP(MOVT); break;
		case 0x79:  SH3_DEC_OP(NOP); break;
		case 0x89:  SH3_DEC_OP(NOP); break;
		case 0x99:  SH3_DEC_OP(DIV0U); break;
		case 0xa9:  SH3_DEC_OP(MOVT); break;
		case 0xb9:  SH3_DEC_OP(NOP); break;
		case 0xc9:  SH3_DEC_OP(NOP); break;
		case 0xd9:  SH3_DEC_OP(DIV0U); break;
		case 0xe9:  SH3_DEC_OP(MOVT); break;
		case 0xf9:  SH3_DEC_OP(NOP); break;
		// 0xa0
		case 0x0a:  SH3_DEC_OP(STSMACH); break;
		case 0x1a:  SH3_DEC_OP(STSMACL); break;
		case 0x2a:  SH3_DEC_OP(STSPR); break;
		case 0x3a:  SH3_DEC_OP(STCSGR); break;
		case 0x4a:  SH3_DEC_OP(NOP); break;
		case 0x5a:  SH3_DEC_OP(STSFPUL); break;
		case 0x6a:  SH3_DEC_OP(STSFPSCR); break;
		case 0x7a:  SH3_DEC_OP(STCDBR); break;
		case 0x8a:  SH3_DEC_OP(STSMACH); break;
		case 0x9a:  SH3_DEC_OP(STSMACL); break;
		case 0xaa:  SH3_DEC_OP(STSPR); break;
		case 0xba:  SH3_DEC_OP(STCSGR); break;
		case 0xca:  SH3_DEC_OP(NOP); break;
		case 0xda:  SH3_DEC_OP(STSFPUL); break;
		case 0xea:  SH3_DEC_OP(STSFPSCR); break;
		case 0xfa:  SH3_DEC_OP(STCDBR); break;
		// 0xb0
		case 0x0b:  SH3_DEC_OP(RTS); break;
		case 0x1b:  SH3_DEC_OP(SLEEP); break;
		case 0x2b:  SH3_DEC_OP(RTE); break;
		case 0x3b:  SH3_DEC_OP(NOP); break;
		case 0x4b:  SH3_DEC_OP(RTS); break;
		case 0x5b:  SH3_DEC_OP(SLEEP); break;
		case 0x6b:  SH3_DEC_OP(RTE); break;
		case 0x7b:  SH3_DEC_OP(NOP); break;
		case 0x8b:  SH3_DEC_OP(RTS); break;
		case 0x9b:  SH3_DEC_OP(SLEEP); break;
		case 0xab:  SH3_DEC_OP(RTE); break;
		case 0xbb:  SH3_DEC_OP(NOP); break;
		case 0xcb:  SH3_DEC_OP(RTS); break;
		case 0xdb:  SH3_DEC_OP(SLEEP); break;
		case 0xeb:  SH3_DEC_OP(RTE); break;
		case 0xfb:  SH3_DEC_OP(NOP); break;
		// 0xc0
		case 0x0c:  SH3_DEC_OP(MOVBL0); break;
		case 0x1c:  SH3_DEC_OP(MOVBL0); break;
		case 0x2c:  SH3_DEC_OP(MOVBL0); break;
		case 0x3c:  SH3_DEC_OP(MOVBL0); break;
		case 0x4c:  SH3_DEC_OP(MOVBL0); break;
		case 0x5c:  SH3_DEC_OP(MOVBL0); break;
		case 0x6c:  SH3_DEC_OP(MOVBL0); break;
		case 0x7c:  SH3_DEC_OP(MOVBL0); break;
		case 0x8c:  SH3_DEC_OP(MOVBL0); break;
		case 0x9c:  SH3_DEC_OP(MOVBL0); break;
		case 0xac:  SH3_DEC_OP(MOVBL0); break;
		case 0xbc:  SH3_DEC_OP(MOVBL0); break;
		case 0xcc:  SH3_DEC_OP(MOVBL0); break;
		case 0xdc:  SH3_DEC_OP(MOVBL0); break;
		case 0xec:  SH3_DEC_OP(MOVBL0); break;
		case 0xfc:  SH3_DEC_OP(MOVBL0); break;
		// 0xd0
		case 0x0d:  SH3_DEC_OP(MOVWL0); break;
		case 0x1d:  SH3_DEC_OP(MOVWL0); break;
		case 0x2d:  SH3_DEC_OP(MOVWL0); break;
		case 0x3d:  SH3_DEC_OP(MOVWL0); break;
		case 0x4d:  SH3_DEC_OP(MOVWL0); break;
		case 0x5d:  SH3_DEC_OP(MOVWL0); break;
		case 0x6d:  SH3_DEC_OP(MOVWL0); break;
		case 0x7d:  SH3_DEC_OP(MOVWL0); break;
		case 0x8d:  SH3_DEC_OP(MOVWL0); break;
		case 0x9d:  SH3_DEC_OP(MOVWL0); break;
		case 0xad:  SH3_DEC_OP(MOVWL0); break;
		case 0xbd:  SH3_DEC_OP(MOVWL0); break;
		case 0xcd:  SH3_DEC_OP(MOVWL0); break;
		case 0xdd:  SH3_DEC_OP(MOVWL0); break;
		case 0xed:  SH3_DEC_OP(MOVWL0); break;
		case 0xfd:  SH3_DEC_OP(MOVWL0); break;
		// 0xe0
		case 0x0e:  SH3_DEC_OP(MOVLL0); break;
		case 0x1e:  SH3_DEC_OP(MOVLL0); break;
		case 0x2e:  SH3_DEC_OP(MOVLL0); break;
		case 0x3e:  SH3_DEC_OP(MOVLL0); break;
		case 0x4e:  SH3_DEC_OP(MOVLL0); break;
		case 0x5e:  SH3_DEC_OP(MOVLL0); break;
		case 0x6e:  SH3_DEC_OP(MOVLL0); break;
		case 0x7e:  SH3_DEC_OP(MOVLL0); break;
		case 0x8e:  SH3_DEC_OP(MOVLL0); break;
		case 0x9e:  SH3_DEC_OP(MOVLL0); break;
		case 0xae:  SH3_DEC_OP(MOVLL0); break;
		case 0xbe:  SH3_DEC_OP(MOVLL0); break;
		case 0xce:  SH3_DEC_OP(MOVLL0); break;
		case 0xde:  SH3_DEC_OP(MOVLL0); break;
		case 0xee:  SH3_DEC_OP(MOVLL0); break;
		case 0xfe:  SH3_DEC_OP(MOVLL0); break;
		// 0xf0
		case 0x0f:  SH3_DEC_OP(MAC_L); break;
		case 0x1f:  SH3_DEC_OP(MAC_L); break;
		case 0x2f:  SH3_DEC_OP(MAC_L); break;
		case 0x3f:  SH3_DEC_OP(MAC_L); break;
		case 0x4f:  SH3_DEC_OP(MAC_L); break;
		case 0x5f:  SH3_DEC_OP(MAC_L); break;
		case 0x6f:  SH3_DEC_OP(MAC_L); break;
		case 0x7f:  SH3_DEC_OP(MAC_L); break;
		case 0x8f:  SH3_DEC_OP(MAC_L); break;
		case 0x9f:  SH3_DEC_OP(MAC_L); break;
		case 0xaf:  SH3_DEC_OP(MAC_L); break;
		case 0xbf:  SH3_DEC_OP(MAC_L); break;
		case 0xcf:  SH3_DEC_OP(MAC_L); break;
		case 0xdf:  SH3_DEC_OP(MAC_L); break;
		case 0xef:  SH3_DEC_OP(MAC_L); break;
		case 0xff:  SH3_DEC_OP(MAC_L); break;
	}
	SH3_DEC_DEFAULT();
}

static inline SH3_DEC_RET SH3_DEC_NAME(execute_one_4000)(const UINT16 opcode)
{
	switch(opcode & 0xff)
	{
		// 0x00
		case 0x00:  SH3_DEC_OP(SHLL); break;
		case 0x10:  SH3_DEC_OP(DT); break;
		case 0x20:  SH3_DEC_OP(SHAL); break;
		case 0x30:  SH3_DEC_OP(NOP); break;
		case 0x40:  SH3_DEC_OP(SHLL); break;
		case 0x50:  SH3_DEC_OP(DT); break;
		case 0x60:  SH3_DEC_OP(SHAL); break;
		case 0x70:  SH3_DEC_OP(NOP); break;
		case 0x80:  SH3_DEC_OP(SHLL); break;
		case 0x90:  SH3_DEC_OP(DT); break;
		case 0xa0:  SH3_DEC_OP(SHAL); break;
		case 0xb0:  SH3_DEC_OP(NOP); break;
		case 0xc0:  SH3_DEC_OP(SHLL); break;
		case 0xd0:  SH3_DEC_OP(DT); break;
		case 0xe0:  SH3_DEC_OP(SHAL); break;
		case 0xf0:  SH3_DEC_OP(NOP); break;
		// 0x10
		case 0x01:  SH3_DEC_OP(SHLR); break;
		case 0x11:  SH3_DEC_OP(CMPPZ); break;
		case 0x21:  SH3_DEC_OP(SHAR); break;
		case 0x31:  SH3_DEC_OP(NOP); break;
		case 0x41:  SH3_DEC_OP(SHLR); break;
		case 0x51:  SH3_DEC_OP(CMPPZ); break;
		case 0x61:  SH3_DEC_OP(SHAR); break;
		case 0x71:  SH3_DEC_OP(NOP); break;
		case 0x81:  SH3_DEC_OP(SHLR); break;
		case 0x91:  SH3_DEC_OP(CMPPZ); break;
		case 0xa1:  SH3_DEC_OP(SHAR); break;
		case 0xb1:  SH3_DEC_OP(NOP); break;
		case 0xc1:  SH3_DEC_OP(SHLR); break;
		case 0xd1:  SH3_DEC_OP(CMPPZ); break;
		case 0xe1:  SH3_DEC_OP(SHAR); break;
		case 0xf1:  SH3_DEC_OP(NOP); break;
		// 0x20
		case 0x02:  SH3_DEC_OP(STSMMACH); break;
		case 0x12:  SH3_DEC_OP(STSMMACL); break;
		case 0x22:  SH3_DEC_OP(STSMPR); break;
		case 0x32:  SH3_DEC_OP(STCMSGR); break;
		case 0x42:  SH3_DEC_OP(NOP); break;
		case 0x52:  SH3_DEC_OP(STSMFPUL); break;
		case 0x62:  SH3_DEC_OP(STSMFPSCR); break;
		case 0x72:  SH3_DEC_OP(NOP); break;
		case 0x82:  SH3_DEC_OP(NOP); break;
		case 0x92:  SH3_DEC_OP(NOP); break;
		case 0xa2:  SH3_DEC_OP(NOP); break;
		case 0xb2:  SH3_DEC_OP(NOP); break;
		case 0xc2:  SH3_DEC_OP(NOP); break;
		case 0xd2:  SH3_DEC_OP(NOP); break;
		case 0xe2:  SH3_DEC_OP(NOP); break;
		case 0xf2:  SH3_DEC_OP(STCMDBR); break;
		// 0x30
		case 0x03:  SH3_DEC_OP(STCMSR); break;
		case 0x13:  SH3_DEC_OP(STCMGBR); break;
		case 0x23:  SH3_DEC_OP(STCMVBR); break;
		case 0x33:  SH3_DEC_OP(STCMSSR); break;
		case 0x43:  SH3_DEC_OP(STCMSPC); break;
		case 0x53:  SH3_DEC_OP(NOP); break;
		case 0x63:  SH3_DEC_OP(NOP); break;
		case 0x73:  SH3_DEC_OP(NOP); break;
		case 0x83:  SH3_DEC_OP(STCMRBANK); break;
		case 0x93:  SH3_DEC_OP(STCMRBANK); break;
		case 0xa3:  SH3_DEC_OP(STCMRBANK); break;
		case 0xb3:  SH3_DEC_OP(STCMRBANK); break;
		case 0xc3:  SH3_DEC_OP(STCMRBANK); break;
		case 0xd3:  SH3_DEC_OP(STCMRBANK); break;
		case 0xe3:  SH3_DEC_OP(STCMRBANK); break;
		case 0xf3:  SH3_DEC_OP(STCMRBANK); break;
		// 0x40
		case 0x04:  SH3_DEC_OP(ROTL); break;
		case 0x14:  SH3_DEC_OP(NOP); break;
		case 0x24:  SH3_DEC_OP(ROTCL); break;
		case 0x34:  SH3_DEC_OP(NOP); break;
		case 0x44:  SH3_DEC_OP(ROTL); break;
		case 0x54:  SH3_DEC_OP(NOP); break;
		case 0x64:  SH3_DEC_OP(ROTCL); break;
		case 0x74:  SH3_DEC_OP(NOP); break;
		case 0x84:  SH3_DEC_OP(ROTL); break;
		case 0x94:  SH3_DEC_OP(NOP); break;
		case 0xa4:  SH3_DEC_OP(ROTCL); break;
		case 0xb4:  SH3_DEC_OP(NOP); break;
		case 0xc4:  SH3_DEC_OP(ROTL); break;
		case 0xd4:  SH3_DEC_OP(NOP); break;
		case 0xe4:  SH3_DEC_OP(ROTCL); break;
		case 0xf4:  SH3_DEC_OP(NOP); break;
		// 0x50
		case 0x05:  SH3_DEC_OP(ROTR); break;
		case 0x15:  SH3_DEC_OP(CMPPL); break;
		case 0x25:  SH3_DEC_OP(ROTCR); break;
		case 0x35:  SH3_DEC_OP(NOP); break;
		case 0x45:  SH3_DEC_OP(ROTR); break;
		case 0x55:  SH3_DEC_OP(CMPPL); break;
		case 0x65:  SH3_DEC_OP(ROTCR); break;
		case 0x75:  SH3_DEC_OP(NOP); break;
		case 0x85:  SH3_DEC_OP(ROTR); break;
		case 0x95:  SH3_DEC_OP(CMPPL); break;
		case 0xa5:  SH3_DEC_OP(ROTCR); break;
		case 0xb5:  SH3_DEC_OP(NOP); break;
		case 0xc5:  SH3_DEC_OP(ROTR); break;
		case 0xd5:  SH3_DEC_OP(CMPPL); break;
		case 0xe5:  SH3_DEC_OP(ROTCR); break;
		case 0xf5:  SH3_DEC_OP(NOP); break;
		// 0x60
		case 0x06:  SH3_DEC_OP(LDSMMACH); break;
		case 0x16:  SH3_DEC_OP(LDSMMACL); break;
		case 0x26:  SH3_DEC_OP(LDSMPR); break;
		case 0x36:  SH3_DEC_OP(NOP); break;
		case 0x46:  SH3_DEC_OP(NOP); break;
		case 0x56:  SH3_DEC_OP(LDSMFPUL); break;
		case 0x66:  SH3_DEC_OP(LDSMFPSCR); break;
		case 0x76:  SH3_DEC_OP(NOP); break;
		case 0x86:  SH3_DEC_OP(NOP); break;
		case 0x96:  SH3_DEC_OP(NOP); break;
		case 0xa6:  SH3_DEC_OP(NOP); break;
		case 0xb6:  SH3_DEC_OP(NOP); break;
		case 0xc6:  SH3_DEC_OP(NOP); break;
		case 0xd6:  SH3_DEC_OP(NOP); break;
		case 0xe6:  SH3_DEC_OP(NOP); break;
		case 0xf6:  SH3_DEC_OP(LDCMDBR); break;
		// 0x70
		case 0x07:  SH3_DEC_OP(LDCMSR); break;
		case 0x17:  SH3_DEC_OP(LDCMGBR); break;
		case 0x27:  SH3_DEC_OP(LDCMVBR); break;
		case 0x37:  SH3_DEC_OP(LDCMSSR); break;
		case 0x47:  SH3_DEC_OP(LDCMSPC); break;
		case 0x57:  SH3_DEC_OP(NOP); break;
		case 0x67:  SH3_DEC_OP(NOP); break;
		case 0x77:  SH3_DEC_OP(NOP); break;
		case 0x87:  SH3_DEC_OP(LDCMRBANK); break;
		case 0x97:  SH3_DEC_OP(LDCMRBANK); break;
		case 0xa7:  SH3_DEC_OP(LDCMRBANK); break;
		case 0xb7:  SH3_DEC_OP(LDCMRBANK); break;
		case 0xc7:  SH3_DEC_OP(LDCMRBANK); break;
		case 0xd7:  SH3_DEC_OP(LDCMRBANK); break;
		case 0xe7:  SH3_DEC_OP(LDCMRBANK); break;
		case 0xf7:  SH3_DEC_OP(LDCMRBANK); break;
		// 0x80
		case 0x08:  SH3_DEC_OP(SHLL2); break;
		case 0x18:  SH3_DEC_OP(SHLL8); break;
		case 0x28:  SH3_DEC_OP(SHLL16); break;
		case 0x38:  SH3_DEC_OP(NOP); break;
		case 0x48:  SH3_DEC_OP(SHLL2); break;
		case 0x58:  SH3_DEC_OP(SHLL8); break;
		case 0x68:  SH3_DEC_OP(SHLL16); break;
		case 0x78:  SH3_DEC_OP(NOP); break;
		case 0x88:  SH3_DEC_OP(SHLL2); break;
		case 0x98:  SH3_DEC_OP(SHLL8); break;
		case 0xa8:  SH3_DEC_OP(SHLL16); break;
		case 0xb8:  SH3_DEC_OP(NOP); break;
		case 0xc8:  SH3_DEC_OP(SHLL2); break;
		case 0xd8:  SH3_DEC_OP(SHLL8); break;
		case 0xe8:  SH3_DEC_OP(SHLL16); break;
		case 0xf8:  SH3_DEC_OP(NOP); break;
		// 0x90
		case 0x09:  SH3_DEC_OP(SHLR2); break;
		case 0x19:  SH3_DEC_OP(SHLR8); break;
		case 0x29:  SH3_DEC_OP(SHLR16); break;
		case 0x39:  SH3_DEC_OP(NOP); break;
		case 0x49:  SH3_DEC_OP(SHLR2); break;
		case 0x59:  SH3_DEC_OP(SHLR8); break;
		case 0x69:  SH3_DEC_OP(SHLR16); break;
		case 0x79:  SH3_DEC_OP(NOP); break;
		case 0x89:  SH3_DEC_OP(SHLR2); break;
		case 0x99:  SH3_DEC_OP(SHLR8); break;
		case 0xa9:  SH3_DEC_OP(SHLR16); break;
		case 0xb9:  SH3_DEC_OP(NOP); break;
		case 0xc9:  SH3_DEC_OP(SHLR2); break;
		case 0xd9:  SH3_DEC_OP(SHLR8); break;
		case 0xe9:  SH3_DEC_OP(SHLR16); break;
		case 0xf9:  SH3_DEC_OP(NOP); break;
		// 0xa0
		case 0x0a:  SH3_DEC_OP(LDSMACH); break;
		case 0x1a:  SH3_DEC_OP(LDSMACL); break;
		case 0x2a:  SH3_DEC_OP(LDSPR); break;
		case 0x3a:  SH3_DEC_OP(NOP); break;
		case 0x4a:  SH3_DEC_OP(NOP); break;
		case 0x5a:  SH3_DEC_OP(LDSFPUL); break;
		case 0x6a:  SH3_DEC_OP(LDSFPSCR); break;
		case 0x7a:  SH3_DEC_OP(NOP); break;
		case 0x8a:  SH3_DEC_OP(NOP); break;
		case 0x9a:  SH3_DEC_OP(NOP); break;
		case 0xaa:  SH3_DEC_OP(NOP); break;
		case 0xba:  SH3_DEC_OP(NOP); break;
		case 0xca:  SH3_DEC_OP(NOP); break;
		case 0xda:  SH3_DEC_OP(NOP); break;
		case 0xea:  SH3_DEC_OP(NOP); break;
		case 0xfa:  SH3_DEC_OP(LDCDBR); break;
		// 0xb0
		case 0x0b:  SH3_DEC_OP(JSR); break;
		case 0x1b:  SH3_DEC_OP(TAS); break;
		case 0x2b:  SH3_DEC_OP(JMP); break;
		case 0x3b:  SH3_DEC_OP(NOP); break;
		case 0x4b:  SH3_DEC_OP(JSR); break;
		case 0x5b:  SH3_DEC_OP(TAS); break;
		case 0x6b:  SH3_DEC_OP(JMP); break;
		case 0x7b:  SH3_DEC_OP(NOP); break;
		case 0x8b:  SH3_DEC_OP(JSR); break;
		case 0x9b:  SH3_DEC_OP(TAS); break;
		case 0xab:  SH3_DEC_OP(JMP); break;
		case 0xbb:  SH3_DEC_OP(NOP); break;
		case 0xcb:  SH3_DEC_OP(JSR); break;
		case 0xdb:  SH3_DEC_OP(TAS); break;
		case 0xeb:  SH3_DEC_OP(JMP); break;
		case 0xfb:  SH3_DEC_OP(NOP); break;
		// 0xc0
		case 0x0c:  SH3_DEC_OP(SHAD); break;
		case 0x1c:  SH3_DEC_OP(SHAD); break;
		case 0x2c:  SH3_DEC_OP(SHAD); break;
		case 0x3c:  SH3_DEC_OP(SHAD); break;
		case 0x4c:  SH3_DEC_OP(SHAD); break;
		case 0x5c:  SH3_DEC_OP(SHAD); break;
		case 0x6c:  SH3_DEC_OP(SHAD); break;
		case 0x7c:  SH3_DEC_OP(SHAD); break;
		case 0x8c:  SH3_DEC_OP(SHAD); break;
		case 0x9c:  SH3_DEC_OP(SHAD); break;
		case 0xac:  SH3_DEC_OP(SHAD); break;
		case 0xbc:  SH3_DEC_OP(SHAD); break;
		case 0xcc:  SH3_DEC_OP(SHAD); break;
		case 0xdc:  SH3_DEC_OP(SHAD); break;
		case 0xec:  SH3_DEC_OP(SHAD); break;
		case 0xfc:  SH3_DEC_OP(SHAD); break;
		// 0xd0
		case 0x0d:  SH3_DEC_OP(SHLD); break;
		case 0x1d:  SH3_DEC_OP(SHLD); break;
		case 0x2d:  SH3_DEC_OP(SHLD); break;
		case 0x3d:  SH3_DEC_OP(SHLD); break;
		case 0x4d:  SH3_DEC_OP(SHLD); break;
		case 0x5d:  SH3_DEC_OP(SHLD); break;
		case 0x6d:  SH3_DEC_OP(SHLD); break;
		case 0x7d:  SH3_DEC_OP(SHLD); break;
		case 0x8d:  SH3_DEC_OP(SHLD); break;
		case 0x9d:  SH3_DEC_OP(SHLD); break;
		case 0xad:  SH3_DEC_OP(SHLD); break;
		case 0xbd:  SH3_DEC_OP(SHLD); break;
		case 0xcd:  SH3_DEC_OP(SHLD); break;
		case 0xdd:  SH3_DEC_OP(SHLD); break;
		case 0xed:  SH3_DEC_OP(SHLD); break;
		case 0xfd:  SH3_DEC_OP(SHLD); break;
		// 0xe0
		case 0x0e:  SH3_DEC_OP(LDCSR); break;
		case 0x1e:  SH3_DEC_OP(LDCGBR); break;
		case 0x2e:  SH3_DEC_OP(LDCVBR); break;
		case 0x3e:  SH3_DEC_OP(LDCSSR); break;
		case 0x4e:  SH3_DEC_OP(LDCSPC); break;
		case 0x5e:  SH3_DEC_OP(NOP); break;
		case 0x6e:  SH3_DEC_OP(NOP); break;
		case 0x7e:  SH3_DEC_OP(NOP); break;
		case 0x8e:  SH3_DEC_OP(LDCRBANK); break;
		case 0x9e:  SH3_DEC_OP(LDCRBANK); break;
		case 0xae:  SH3_DEC_OP(LDCRBANK); break;
		case 0xbe:  SH3_DEC_OP(LDCRBANK); break;
		case 0xce:  SH3_DEC_OP(LDCRBANK); break;
		case 0xde:  SH3_DEC_OP(LDCRBANK); break;
		case 0xee:  SH3_DEC_OP(LDCRBANK); break;
		case 0xfe:  SH3_DEC_OP(LDCRBANK); break;
		// 0xf0
		case 0x0f:  SH3_DEC_OP(MAC_W); break;
		case 0x1f:  SH3_DEC_OP(MAC_W); break;
		case 0x2f:  SH3_DEC_OP(MAC_W); break;
		case 0x3f:  SH3_DEC_OP(MAC_W); break;
		case 0x4f:  SH3_DEC_OP(MAC_W); break;
		case 0x5f:  SH3_DEC_OP(MAC_W); break;
		case 0x6f:  SH3_DEC_OP(MAC_W); break;
		case 0x7f:  SH3_DEC_OP(MAC_W); break;
		case 0x8f:  SH3_DEC_OP(MAC_W); break;
		case 0x9f:  SH3_DEC_OP(MAC_W); break;
		case 0xaf:  SH3_DEC_OP(MAC_W); break;
		case 0xbf:  SH3_DEC_OP(MAC_W); break;
		case 0xcf:  SH3_DEC_OP(MAC_W); break;
		case 0xdf:  SH3_DEC_OP(MAC_W); break;
		case 0xef:  SH3_DEC_OP(MAC_W); break;
		case 0xff:  SH3_DEC_OP(MAC_W); break;
	}
	SH3_DEC_DEFAULT();
}


static inline SH3_DEC_RET SH3_DEC_NAME(execute_one)(const UINT16 opcode)
{
	switch(opcode & 0xf000)
	{
		case 0x0000:
			SH3_DEC_SUB(execute_one_0000);
			break;

		case 0x1000:
			SH3_DEC_OP(MOVLS4);
			break;

		case 0x2000:
			switch(opcode & 0x0f)
			{
				case 0x00:  SH3_DEC_OP(MOVBS); break;
				case 0x01:  SH3_DEC_OP(MOVWS); break;
				case 0x02:  SH3_DEC_OP(MOVLS); break;
				case 0x03:  SH3_DEC_OP(NOP); break;
				case 0x04:  SH3_DEC_OP(MOVBM); break;
				case 0x05:  SH3_DEC_OP(MOVWM); break;
				case 0x06:  SH3_DEC_OP(MOVLM); break;
				case 0x07:  SH3_DEC_OP(DIV0S); break;
				case 0x08:  SH3_DEC_OP(TST); break;
				case 0x09:  SH3_DEC_OP(AND); break;
				case 0x0a:  SH3_DEC_OP(XOR); break;
				case 0x0b:  SH3_DEC_OP(OR); break;
				case 0x0c:  SH3_DEC_OP(CMPSTR); break;
				case 0x0d:  SH3_DEC_OP(XTRCT); break;
				case 0x0e:  SH3_DEC_OP(MULU); break;
				case 0x0f:  SH3_DEC_OP(MULS); break;
			}
			break;

		case 0x3000:
			switch(opcode & 0x0f)
			{
				case 0x00:  SH3_DEC_OP(CMPEQ); break;
				case 0x01:  SH3_DEC_OP(NOP); break;
				case 0x02:  SH3_DEC_OP(CMPHS); break;
				case 0x03:  SH3_DEC_OP(CMPGE); break;
				case 0x04:  SH3_DEC_OP(DIV1); break;
				case 0x05:  SH3_DEC_OP(DMULU); break;
				case 0x06:  SH3_DEC_OP(CMPHI); break;
				case 0x07:  SH3_DEC_OP(CMPGT); break;
				case 0x08:  SH3_DEC_OP(SUB); break;
				case 0x09:  SH3_DEC_OP(NOP); break;
				case 0x0a:  SH3_DEC_OP(SUBC); break;
				case 0x0b:  SH3_DEC_OP(SUBV); break;
				case 0x0c:  SH3_DEC_OP(ADD); break;
				case 0x0d:  SH3_DEC_OP(DMULS); break;
				case 0x0e:  SH3_DEC_OP(ADDC); break;
				case 0x0f:  SH3_DEC_OP(ADDV); break;
			}
			break;

		case 0x4000:
			SH3_DEC_SUB(execute_one_4000);
			break;

		case 0x5000:
			SH3_DEC_OP(MOVLL4);
			break;

		case 0x6000:
			switch(opcode & 0x0f)
			{
				case 0x00:  SH3_DEC_OP(MOVBL); break;
				case 0x01:  SH3_DEC_OP(MOVWL); break;
				case 0x02:  SH3_DEC_OP(MOVLL); break;
				case 0x03:  SH3_DEC_OP(MOV); break;
				case 0x04:  SH3_DEC_OP(MOVBP); break;
				case 0x05:  SH3_DEC_OP(MOVWP); break;
				case 0x06:  SH3_DEC_OP(MOVLP); break;
				case 0x07:  SH3_DEC_OP(NOT); break;
				case 0x08:  SH3_DEC_OP(SWAPB); break;
				case 0x09:  SH3_DEC_OP(SWAPW); break;
				case 0x0a:  SH3_DEC_OP(NEGC); break;
				case 0x0b:  SH3_DEC_OP(NEG); break;
				case 0x0c:  SH3_DEC_OP(EXTUB); break;
				case 0x0d:  SH3_DEC_OP(EXTUW); break;
				case 0x0e:  SH3_DEC_OP(EXTSB); break;
				case 0x0f:  SH3_DEC_OP(EXTSW); break;
			}
			break;

		case 0x7000:
			SH3_DEC_OP(ADDI);
			break;

		case 0x8000:
			switch((opcode >> 8) & 0x0f)
			{
				case 0x00:  SH3_DEC_OP(MOVBS4); break;
				case 0x01:  SH3_DEC_OP(MOVWS4); break;
				case 0x02:  SH3_DEC_OP(NOP); break;
				case 0x03:  SH3_DEC_OP(NOP); break;
				case 0x04:  SH3_DEC_OP(MOVBL4); break;
				case 0x05:  SH3_DEC_OP(MOVWL4); break;
				case 0x06:  SH3_DEC_OP(NOP); break;
				case 0x07:  SH3_DEC_OP(NOP); break;
				case 0x08:  SH3_DEC_OP(CMPIM); break;
				case 0x09:  SH3_DEC_OP(BT); break;
				case 0x0a:  SH3_DEC_OP(NOP); break;
				case 0x0b:  SH3_DEC_OP(BF); break;
				case 0x0c:  SH3_DEC_OP(NOP); break;
				case 0x0d:  SH3_DEC_OP(BTS); break;
				case 0x0e:  SH3_DEC_OP(NOP); break;
				case 0x0f:  SH3_DEC_OP(BFS); break;
			}
			break;

		case 0x9000:
			SH3_DEC_OP(MOVWI);
			break;

		case 0xa000:
			SH3_DEC_OP(BRA);
			break;

		case 0xb000:
			SH3_DEC_OP(BSR);
			break;

		case 0xc000:
			switch((opcode >> 8) & 0x0f)
			{
				case 0x00:  SH3_DEC_OP(MOVBSG); break;
				case 0x01:  SH3_DEC_OP(MOVWSG); break;
				case 0x02:  SH3_DEC_OP(MOVLSG); break;
				case 0x03:  SH3_DEC_OP(TRAPA); break;
				case 0x04:  SH3_DEC_OP(MOVBLG); break;
				case 0x05:  SH3_DEC_OP(MOVWLG); break;
				case 0x06:  SH3_DEC_OP(MOVLLG); break;
				case 0x07:  SH3_DEC_OP(MOVA); break;
				case 0x08:  SH3_DEC_OP(TSTI); break;
				case 0x09:  SH3_DEC_OP(ANDI); break;
				case 0x0a:  SH3_DEC_OP(XORI); break;
				case 0x0b:  SH3_DEC_OP(ORI); break;
				case 0x0c:  SH3_DEC_OP(TSTM); break;
				case 0x0d:  SH3_DEC_OP(ANDM); break;
				case 0x0e:  SH3_DEC_OP(XORM); break;
				case 0x0f:  SH3_DEC_OP(ORM); break;
			}
			break;

		case 0xd000:
			SH3_DEC_OP(MOVLI);
			break;

		case 0xe000:
			SH3_DEC_OP(MOVI);
			break;

		case 0xf000:
			switch(opcode & 0x0f)
			{
				case 0x00:  SH3_DEC_OP(FADD); break;
				case 0x01:  SH3_DEC_OP(FSUB); break;
				case 0x02:  SH3_DEC_OP(FMUL); break;
				case 0x03:  SH3_DEC_OP(FDIV); break;
				case 0x04:  SH3_DEC_OP(FCMP_EQ); break;
				case 0x05:  SH3_DEC_OP(FCMP_GT); break;
				case 0x06:  SH3_DEC_OP(FMOVS0FR); break;
				case 0x07:  SH3_DEC_OP(FMOVFRS0); break;
				case 0x08:  SH3_DEC_OP(FMOVMRFR); break;
				case 0x09:  SH3_DEC_OP(FMOVMRIFR); break;
				case 0x0a:  SH3_DEC_OP(FMOVFRMR); break;
				case 0x0b:  SH3_DEC_OP(FMOVFRMDR); break;
				case 0x0c:  SH3_DEC_OP(FMOVFR); break;
				case 0x0d:  SH3_DEC_OP(op1111_0x13); break;
				case 0x0e:  SH3_DEC_OP(FMAC); break;
				case 0x0f:  SH3_DEC_OP(dbreak); break;
			}
			break;
	}
	SH3_DEC_DEFAULT();
}
