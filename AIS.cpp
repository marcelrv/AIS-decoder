

#include <AIS.h>

/* Copied from util.h */
#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )
#define htons(x) ( ((x)<<8) | (((x)>>8)&0xFF) )

AIS::AIS(const char *AISbitstream, unsigned int fillBits)
: msgLen(0)
{
	uint8_t* tmp = (uint8_t*)AISbitstream;
	int i = 0;
	while (*tmp != '\0') {
		msg[i] = *tmp;
		i++;
		tmp++;
	}
	msg[i] = *tmp; // Terminate msg

	// Time to decode the AIS data
	decode(fillBits);

	getdata(0,6, &msgType);  // Will be used a lot
}




/*
 * Decode msg into it self. 4 bytes becomes 3, i.e. it is OK to do it
 * cnt    byte
 *  0   --qqqqqq
 *  1   --yyyyyy
 *  2   --zzzzzz
 *  3   --wwwwww
 *
 * becomes | qq qq qq yy | yy yy zz zz | zz ww ww ww |
 */
void AIS::decode(unsigned int fillBits)
{
	unsigned int srcIdx=0;

	/* First convert to binary */
	while (msg[srcIdx] != '\0') {
		msg[srcIdx] -= '0';
		if (msg[srcIdx] > 40) {
			msg[srcIdx] -= 8;
		}
		srcIdx++;
	}
	msgLen = srcIdx; /* For now in bytes - later in bits */

	/* Now compress resulting 6bits values */
	unsigned int dstIdx=0;
	unsigned int cnt=0;
	uint8_t src;

	srcIdx = 0;
	while (srcIdx != msgLen) {
		src = msg[srcIdx];
		switch (cnt) {
		case 0:
			msg[dstIdx] = (src << 2);
			break;
		case 1:
			msg[dstIdx] |= (src >> 4);
			dstIdx++;
			msg[dstIdx] = (src << 4);
			break;
		case 2:
			msg[dstIdx] |= (src >> 2);
			dstIdx++;
			msg[dstIdx] = (src << 6);
			break;
		case 3:
			msg[dstIdx] |= src;
			dstIdx++;
			break;
		}
		srcIdx++;
		cnt++;
		if (cnt==4) {
			cnt = 0;
		}
	}
	/* Store msgLen as bit count */
	msgLen = srcIdx*6 - fillBits;
}

int AIS::getbit(unsigned int idx)
{
	int byteIdx = idx / 8;
	int bitIdx = 7 - (idx % 8);

	return ((msg[byteIdx] >> bitIdx) & 0x01);
}

/*
 * getdata return in data cnt bits from decoded AIS data, starting from begin
 * If not enough data function returns false, otherwise true
 */
bool AIS::getdata(unsigned int begin, unsigned int cnt, uint8_t *data, bool isSigned)
{
	if (begin+cnt > msgLen)
		return false;

	unsigned int srcIdx = begin;
	unsigned int dstIdx = 0;
	unsigned int dstBitIdx = cnt % 8;

	data[dstIdx] = 0;
	if (isSigned) {
		if (getbit(srcIdx) == 1) {
			/* Pre-pend with 1's */
			const static uint8_t signmask[8] =
				{ 0x00, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80};

			data[dstIdx] = signmask[dstBitIdx];
		}
	}

	if (dstBitIdx == 0)
		dstBitIdx = 8;

	while (srcIdx < begin + cnt) {
		dstBitIdx--;
		data[dstIdx] |= (getbit(srcIdx) << dstBitIdx);
		srcIdx++;
		if (0 == dstBitIdx) {
			dstBitIdx = 8;
			dstIdx++;
			data[dstIdx] = 0;
		}
	}

	return true;
}

unsigned long AIS::get_u32(unsigned start, unsigned len)
{
	union {
		uint8_t data[4];
		unsigned long val;
	} u;

	if (start + len > msgLen) return 0;
	getdata(start, len, u.data);
	u.val = htonl(u.val);
	return u.val;
}

long AIS::get_i32(unsigned start, unsigned len)
{
	union {
		uint8_t data[4];
		long val;
	} u;

	if (start + len > msgLen) return 0;
	u.val = htonl(u.val);
	return u.val;
	getdata(start, len, u.data, true);
}

unsigned int AIS::get_u16(unsigned start, unsigned len)
{
	union {
		uint8_t data[2];
		unsigned int val;
	} u;

	if (start + len > msgLen) return 0;
	getdata(start, len, u.data);
	u.val = htons(u.val);
	return u.val;

}

uint8_t AIS::get_u8(unsigned start, unsigned len)
{
	uint8_t val;

	if (start + len > msgLen) return 0;
	getdata(start,len, &val);
	return val;
}

int8_t AIS::get_i8(unsigned start, unsigned len)
{
	union {
		uint8_t data[1];
		int8_t val;
	} u;

	if (start + len > msgLen) return 0;
	getdata(start,len, u.data, true);
	return u.val;
}

int8_t AIS::get_rot()
{
	unsigned int start, len;
	switch (msgType) {
	case 1:
	case 2:
	case 3:
		start = 42;
		len = 8;
		break;
	default:
		return 0;
	}
	return get_i8(start,len);
}

uint8_t AIS::get_repeat()
{
	return get_u8(6,2);

}

uint8_t AIS::get_navStatus()
{
	unsigned int start, len;
	switch (msgType) {
	case 1:
	case 2:
	case 3:
		start = 38;
		len = 4;
		break;
	default:
		return 15; // Not defined
	}
	return get_u8(start,len);
}

uint8_t AIS::get_posAccuracy()
{
	unsigned int start;
	unsigned int len=1;

	switch (msgType) {
	case 1:
	case 2:
	case 3:
		start = 60;
		break;
	case 18:
		start = 56;
		break;
	default:
		return 0; // Not defined
	}
	return get_u8(start,len);
}

uint8_t AIS::get_type()
{
	return msgType;
}


unsigned long AIS::get_mmsi()
{
	return get_u32(8,30);
}

long AIS::get_latitude()
{
	unsigned int start;
	unsigned int len  = 27;
	switch (msgType) {
	case 1:
	case 2:
	case 3:
		start=89;
		break;
	case 18:
		start=85;
		break;
	default:
		return 0;
	}
	return get_i32(start,len);
}

long AIS::get_longitude()
{
	unsigned int start;
	unsigned int len = 28;
	switch(msgType) {
	case 1:
	case 2:
	case 3:
		start = 61;
		break;
	case 18:
		start = 57;
		break;
	default:
		return 0;
	}
	return get_i32(start,len);
}

unsigned int AIS::get_SOG()
{
	unsigned int start;
	unsigned int len = 10;
	switch(msgType) {
	case 1:
	case 2:
	case 3:
		start = 50;
		break;
	case 18:
		start = 46;
		break;
	default:
		return 0;
	}
	return get_u16(start, len);
}

unsigned int AIS::get_COG()
{
	unsigned int start;
	unsigned int len = 12;
	switch (msgType) {
	case 1:
	case 2:
	case 3:
		start = 116;
		break;
	case 18:
		start = 112;
		break;
	default:
		return 0;
	}
	return get_u16(start, len);
}

unsigned int AIS::get_HDG()
{
	unsigned int start;
	unsigned int len=9;
	switch(msgType) {
	case 1:
	case 2:
	case 3:
		start = 128;
		break;
	case 18:
		start = 124;
		break;
	default:
		return 511;
	}
	return get_u16(start,len);
}

uint8_t AIS::get_timeStamp()
{
	unsigned int start;
	unsigned int len=6;
	switch(msgType) {
	case 1:
	case 2:
	case 3:
		start = 137;
		break;
	case 18:
		start = 133;
		break;
	default:
		return 0;
	}
	return get_u8(start,len);
}

uint8_t AIS::get_manIndicator()
{
	unsigned int start,len;
	switch(msgType) {
	case 1:
	case 2:
	case 3:
		start = 143;
		len = 2;
		break;
	default:
		return 0;
	}
	return get_u8(start,len);
}

uint8_t AIS::get_raim()
{
	unsigned int start;
	unsigned int len = 1;
	switch(msgType) {
	case 1:
	case 2:
	case 3:
		start = 148;
		break;
	case 18:
		start = 147;
		break;
	default:
		return 0;
	}
	return get_u8(start,len);
}
