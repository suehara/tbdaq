// Listener.cc

#include "Listener.h"
#include "decoder.h"

bool TScCalListener::Process(const std::vector<unsigned char> &buf, int nwrite)
{
	struct data_t data[16*36*8];
	int len = 0;
	unpackRaw(_cycle++, &buf.front(), nwrite, data, &len);
	
	return len!=0;
}
